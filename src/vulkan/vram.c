#define INCLUDE_VULKAN_INTERNAL
#include "vulkan.h"

extern const VulkanContext* getVulkanContextPtr(void);
extern const QueueContext* getQueueContextPtr(void);

typedef struct {
    VkDeviceMemory memory;
    u64 size;
    u32 type_id;
    u32 flags;
} VramBlock;

typedef struct {
    u64 offset;
    u64 size;
} VramAllocation;

static VramBlock s_vram_blocks[MEMORY_BLOCK_COUNT] = {0};
static EventCallback s_callback = NULL;

static void* s_vram_allocation_buffer = NULL;
static VramAllocation* s_vram_allocations = NULL; // = s_vram_allocation_buffer
static VramAllocation* s_vram_free_spaces = NULL; // = (VramAllocation*)s_vram_allocation_buffer + MEMORY_BLOCK_COUNT * MEMORY_BLOCK_MAX_ALLOCATIONS 
static u32* s_vram_allocation_counts = NULL;
static u32* s_vram_free_space_counts = NULL;

#define VRAM_ALLOCATION(array, block_id, local_id) array[block_id * MEMORY_BLOCK_MAX_ALLOCATIONS + local_id]
#define VRAM_ALLOCATION_COUNT(array, block_id) array[block_id]

#define PACK_MEMORY_HANDLE(handle, block, alloc) handle = PACK_32_TO_64(block + 1, alloc + 1);
#define UNPACK_MEMORY_HANDLE(handle, block, alloc) block = UNPACK_64_TO_32_A(handle) - 1; alloc = UNPACK_64_TO_32_B(handle) - 1;

b32 layoutDeviceMemory(VkPhysicalDevice device, u32 block_count, const MemoryBlockDscr* block_dscrs, VramBlock* vram_blocks) {
    const u64 reserved_space = 1024 * 1024;
    VkPhysicalDeviceMemoryProperties device_memory;
    vkGetPhysicalDeviceMemoryProperties(device, &device_memory);

    for(u32 i = 0; i < block_count; i++) {
        u64 block_size = block_dscrs[i].size;
        u32 pos_flag = block_dscrs[i].positive_flags;
        u32 neg_flag = block_dscrs[i].negative_flags;
        for(u32 j = 0; j < device_memory.memoryTypeCount; j++) {
            u32 heap_id = device_memory.memoryTypes[j].heapIndex;
            u32 memory_flags = device_memory.memoryTypes[j].propertyFlags;
            if(
                device_memory.memoryHeaps[heap_id].size >= reserved_space + block_size &&
                FLAG_IN_MASK(memory_flags, pos_flag) && FLAG_NOT_IN_MASK(memory_flags, neg_flag)
            ) {
                device_memory.memoryHeaps[heap_id].size -= block_size;
                vram_blocks[i].size = block_size;
                vram_blocks[i].type_id = j;
                vram_blocks[i].flags = memory_flags;
                goto _found;
            }
        }
//_not_found:
        return FALSE;
_found:
    }
    return TRUE;
}

#define _INVOKE_CALLBACK(code)                                                  \
return_code = MAKE_VK_ERR(code);                                                \
if(s_callback(return_code)) {                                                   \
    goto _end;                                                                  \
}

result vramInit(EventCallback callback) {
    VulkanContext vulkan_context = *getVulkanContextPtr();
    result return_code = CODE_SUCCESS;
    s_callback = callback ? callback : vulkan_context.callback;

    ERROR_CATCH(!layoutDeviceMemory(vulkan_context.physical_device, MEMORY_BLOCK_COUNT, (MemoryBlockDscr[])MEMORY_BLOCK_DESCRIPTORS, s_vram_blocks)) {
        _INVOKE_CALLBACK(VK_ERR_VRAM_LAYOUT_FAIL)
    }

    VkMemoryAllocateInfo alloc_info = (VkMemoryAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    };
    for(u32 i = 0; i < MEMORY_BLOCK_COUNT; i++) {
        alloc_info.allocationSize = s_vram_blocks[i].size;
        alloc_info.memoryTypeIndex = s_vram_blocks[i].type_id;
        ERROR_CATCH(vkAllocateMemory(vulkan_context.device, &alloc_info, NULL, &s_vram_blocks[i].memory) != VK_SUCCESS) {
            _INVOKE_CALLBACK(VK_ERR_VRAM_ALLOCATE);
        }
    }

    s_vram_allocation_buffer = malloc((sizeof(VramAllocation) * MEMORY_BLOCK_MAX_ALLOCATIONS + sizeof(u32)) * MEMORY_BLOCK_COUNT * 2);
    ERROR_CATCH(!s_vram_allocation_buffer) {
        _INVOKE_CALLBACK(VK_ERR_VRAM_ALLOCATION_BUFFER_ALLOCATE)
    }
    s_vram_allocations = s_vram_allocation_buffer;
    s_vram_free_spaces = (VramAllocation*)s_vram_allocation_buffer + MEMORY_BLOCK_COUNT * MEMORY_BLOCK_MAX_ALLOCATIONS;
    s_vram_allocation_counts = (u32*)((char*)s_vram_allocation_buffer + sizeof(VramAllocation) * MEMORY_BLOCK_MAX_ALLOCATIONS * MEMORY_BLOCK_COUNT * 2);
    s_vram_free_space_counts = (u32*)((char*)s_vram_allocation_buffer + (sizeof(VramAllocation) * MEMORY_BLOCK_MAX_ALLOCATIONS * 2 + sizeof(u32)) * MEMORY_BLOCK_COUNT);
    for(u32 i = 0; i < MEMORY_BLOCK_COUNT; i++) {
        VRAM_ALLOCATION(s_vram_allocations, i, 0) = (VramAllocation){0};
        VRAM_ALLOCATION(s_vram_free_spaces, i, 0) = (VramAllocation) {
            .offset = 0,
            .size = s_vram_blocks[i].size
        };
        VRAM_ALLOCATION_COUNT(s_vram_allocation_counts, i) = 0;
        VRAM_ALLOCATION_COUNT(s_vram_free_space_counts, i) = 1;
        for(u32 j = 1; j < MEMORY_BLOCK_MAX_ALLOCATIONS; j++) {
            VRAM_ALLOCATION(s_vram_allocations, i, j) = (VramAllocation){0};
            VRAM_ALLOCATION(s_vram_free_spaces, i, j) = (VramAllocation){0};
        };
    }

_end:
    return return_code;
}

void vramTerminate(void) {
    VulkanContext vulkan_context = *getVulkanContextPtr();
    SAFE_DESTROY(s_vram_allocation_buffer, free(s_vram_allocation_buffer));
    s_vram_allocations = s_vram_free_spaces = NULL;
    s_vram_allocation_counts = s_vram_free_space_counts = NULL;
    for(u32 i = 0; i < MEMORY_BLOCK_COUNT; i++) {
        SAFE_DESTROY(s_vram_blocks[i].memory, vkFreeMemory(vulkan_context.device, s_vram_blocks[i].memory, NULL))
        s_vram_blocks[i] = (VramBlock){0};
    }
    vulkan_context = (VulkanContext){0};
    s_callback = NULL;
}

result vramAllocate(u64 size, u64 aligment, u32 block_id, u64* const handle) {
    ERROR_CATCH(size > s_vram_blocks[block_id].size) {
        return VRAM_ALLOCATE_ALLOCATION_BIGGER_THAN_BLOCK;
    }
    u32 allocation_count = VRAM_ALLOCATION_COUNT(s_vram_allocation_counts, block_id);
    ERROR_CATCH(allocation_count == MEMORY_BLOCK_MAX_ALLOCATIONS) {
        return VRAM_ALLOCATE_TOO_MANY_ALLOCATIONS;
    }
    u32 free_space_count = VRAM_ALLOCATION_COUNT(s_vram_free_space_counts, block_id);
    u32 space_id;
    VramAllocation unaligned_space, aligned_space;
    for(space_id = 0; space_id < free_space_count; space_id++) {
        unaligned_space = VRAM_ALLOCATION(s_vram_free_spaces, block_id, space_id);
        aligned_space.offset = ALIGN(unaligned_space.offset, aligment);
        aligned_space.size = unaligned_space.size + unaligned_space.offset - aligned_space.offset;
        if(aligned_space.offset < unaligned_space.offset + unaligned_space.size && aligned_space.size >= size) {
            goto _found_block;
        }
    }
    return VRAM_ALLOCATE_FAILED_TO_FIND_FREE_SAPCE;

_found_block:
    size = (aligned_space.size <= size + MEMORY_ALLOCATION_SNAP) ? aligned_space.size : size;
    VramAllocation allocation = (VramAllocation) {
        .offset = aligned_space.offset,
        .size = size
    };
    unaligned_space.size -= aligned_space.size;
    aligned_space.offset += size;
    aligned_space.size -= size;

    if(aligned_space.size == 0) {
        free_space_count--;
        VRAM_ALLOCATION_COUNT(s_vram_free_space_counts, block_id) = free_space_count;
        for(u32 i = 0; i < free_space_count; i++) {
            VRAM_ALLOCATION(s_vram_free_spaces, block_id, i) = VRAM_ALLOCATION(s_vram_free_spaces, block_id, i + (i >= space_id));
        }
    } else {
        u32 insert_space_id = space_id;
        for(u32 i = 0; i < space_id; i++) {
            VramAllocation space = VRAM_ALLOCATION(s_vram_free_spaces, block_id, i);
            if(space.size > aligned_space.size) {
                insert_space_id = i;
                break;
            }
        }
        for(u32 i = 0; i < free_space_count; i++) {
            VRAM_ALLOCATION(s_vram_free_spaces, block_id, i) = VRAM_ALLOCATION(s_vram_free_spaces, block_id, i + (i > space_id) - (i > insert_space_id));
        }
        VRAM_ALLOCATION(s_vram_free_spaces, block_id, insert_space_id) = aligned_space;
    }
    VramAllocation current_allocation;
    u32 alloc_slot_id = U32_MAX;
    u32 connection_slot_id = U32_MAX;
    for(u32 i = 0; i < MEMORY_BLOCK_MAX_ALLOCATIONS; i++) {
        current_allocation = VRAM_ALLOCATION(s_vram_allocations, block_id, i);
        alloc_slot_id = (alloc_slot_id == U32_MAX && current_allocation.size == 0) ? i : alloc_slot_id;
        connection_slot_id = (connection_slot_id == U32_MAX && current_allocation.size != 0 && current_allocation.offset + current_allocation.size == unaligned_space.offset) ? i : connection_slot_id; 
        if(connection_slot_id != U32_MAX && alloc_slot_id != U32_MAX) break;
    }
    if(connection_slot_id != U32_MAX) {
        VRAM_ALLOCATION(s_vram_allocations, block_id, connection_slot_id).size += unaligned_space.size;
    }
    VRAM_ALLOCATION(s_vram_allocations, block_id, alloc_slot_id) = allocation;
    VRAM_ALLOCATION_COUNT(s_vram_allocation_counts, block_id)++;
    PACK_MEMORY_HANDLE(*handle, block_id, alloc_slot_id)
    return VRAM_ALLOCATE_SUCCESS;
}

result vramAllocateBuffers(u32 buffer_count, const VkBuffer* buffers, u32 block_id, u64* const handle) {
    u64 allocation_size = 0;
    u32 type_id = s_vram_blocks[block_id].type_id;

    VkDevice device = getVulkanContextPtr()->device;
    VkMemoryRequirements* memory_requirements = alloca(sizeof(VkMemoryRequirements) * buffer_count);
    u64 max_aligment = 1;
    for(u32 i = 0; i < buffer_count; i++) {
        vkGetBufferMemoryRequirements(device, buffers[i], memory_requirements + i);
        ERROR_CATCH(!(BIT(type_id) & memory_requirements[i].memoryTypeBits)) {
            return VRAM_ALLOCATE_WRONG_MEMORY_TYPE;
        }
        allocation_size = ALIGN(allocation_size, memory_requirements[i].alignment) + memory_requirements[i].size;
        max_aligment = MAX(max_aligment, memory_requirements[i].alignment);
    }
    u32 result = vramAllocate(allocation_size, max_aligment, block_id, handle);
    ERROR_CATCH(result != VRAM_ALLOCATE_SUCCESS) {
        return result;
    }
    u32 alloc_id;
    UNPACK_MEMORY_HANDLE(*handle, block_id, alloc_id);
    u64 offset = VRAM_ALLOCATION(s_vram_allocations, block_id, alloc_id).offset;
    for(u32 i = 0; i < buffer_count; i++) {
        offset = ALIGN(offset, memory_requirements[i].alignment);
        ERROR_CATCH(vkBindBufferMemory(device, buffers[i], s_vram_blocks[block_id].memory, offset) != VK_SUCCESS) {
            return VRAM_ALLOCATE_BUFFER_BIND_FAILED;
        }
        offset += memory_requirements[i].size;
    }
    return VRAM_ALLOCATE_SUCCESS;
}

result vramAllocateImages(u32 image_count, const VkImage* images, u32 block_id, u64* const handle) {
    u64 allocation_size = 0;
    u32 type_id = s_vram_blocks[block_id].type_id;

    VkDevice device = getVulkanContextPtr()->device;
    VkMemoryRequirements* memory_requirements = alloca(sizeof(VkMemoryRequirements) * image_count);
    u64 max_aligment = 1;
    for(u32 i = 0; i < image_count; i++) {
        vkGetImageMemoryRequirements(device, images[i], memory_requirements + i);
        ERROR_CATCH(!(BIT(type_id) & memory_requirements[i].memoryTypeBits)) {
            return VRAM_ALLOCATE_WRONG_MEMORY_TYPE;
        }
        allocation_size = ALIGN(allocation_size, memory_requirements[i].alignment) + memory_requirements[i].size;
        max_aligment = MAX(max_aligment, memory_requirements[i].alignment);
    }
    u32 result = vramAllocate(allocation_size, max_aligment, block_id, handle);
    ERROR_CATCH(result != VRAM_ALLOCATE_SUCCESS) {
        return result;
    }
    u32 alloc_id;
    UNPACK_MEMORY_HANDLE(*handle, block_id, alloc_id);
    u64 offset = VRAM_ALLOCATION(s_vram_allocations, block_id, alloc_id).offset;
    for(u32 i = 0; i < image_count; i++) {
        offset = ALIGN(offset, memory_requirements[i].alignment);
        ERROR_CATCH(vkBindImageMemory(device, images[i], s_vram_blocks[block_id].memory, offset) != VK_SUCCESS) {
            return VRAM_ALLOCATE_BUFFER_BIND_FAILED;
        }
        offset += memory_requirements[i].size;
    }
    return VRAM_ALLOCATE_SUCCESS;
}

result vramFree(u64 handle) {
    u32 block_id, alloc_id;
    UNPACK_MEMORY_HANDLE(handle, block_id, alloc_id)

    VramAllocation allocation = VRAM_ALLOCATION(s_vram_allocations, block_id, alloc_id);
    if(!(allocation.size || allocation.offset)) {
        return VRAM_FREE_ALREADY_EMPTY;
    }
    u32 block_allocation_count = VRAM_ALLOCATION_COUNT(s_vram_allocation_counts, block_id) - 1; // copy of count
    u32 block_free_space_count = VRAM_ALLOCATION_COUNT(s_vram_free_space_counts, block_id);

    u32 left_id = U32_MAX;
    u32 right_id = U32_MAX;
    u32 right_exists = 0;
    u32 left_exists = 0;
    u32 space_id = 0;
    for(u32 i = 0; i < block_free_space_count; i++) {
        VramAllocation space = VRAM_ALLOCATION(s_vram_free_spaces, block_id, i);

        left_exists = (space.offset + space.size == allocation.offset);
        right_exists = (allocation.offset + allocation.size == space.offset);
        left_id = left_exists ? i : left_id;
        right_id = right_exists ? i : right_id;

        allocation.offset = left_exists ? space.offset : allocation.offset;
        allocation.size = (right_exists || left_exists) ? allocation.size + space.size : allocation.size;
        space_id = (space.size < allocation.size) ? i + 1 : space_id;
    }
    if(space_id >= MEMORY_BLOCK_MAX_ALLOCATIONS) {
        return VRAM_FREE_TO_MANY_FREE_BLOCKS;
    }
    left_exists = left_id != U32_MAX;
    right_exists = right_id != U32_MAX;
    block_free_space_count += !(right_exists || left_exists) - (right_exists && left_exists); // keep in mind that its a copy of count
    space_id += - (right_exists && (space_id != right_id)) - left_exists;
    for(u32 i = 0; i < block_free_space_count; i++) {
        u32 offset = (right_exists && i > right_id) + (left_exists && i > left_id) - (i > space_id);
        VRAM_ALLOCATION(s_vram_free_spaces, block_id, i) = VRAM_ALLOCATION(s_vram_free_spaces, block_id, i + offset);
    }
    VRAM_ALLOCATION(s_vram_free_spaces, block_id, space_id) = allocation;
    VRAM_ALLOCATION_COUNT(s_vram_free_space_counts, block_id) = block_free_space_count;
    VRAM_ALLOCATION(s_vram_allocations, block_id, alloc_id) = (VramAllocation){0};
    VRAM_ALLOCATION_COUNT(s_vram_allocation_counts, block_id) = block_allocation_count;
    return VRAM_FREE_SUCESS;
}

b32 vramWriteToAllocation(u64 handle, VramWriteDscr* write_dscr) {
    u32 block_id, alloc_id;
    UNPACK_MEMORY_HANDLE(handle, block_id, alloc_id)
    VramAllocation allocation = VRAM_ALLOCATION(s_vram_allocations, block_id, alloc_id);
    VkDevice device = getVulkanContextPtr()->device;
    VkDeviceMemory memory = s_vram_blocks[block_id].memory;
    void* map_ptr;
    allocation.offset += write_dscr->offset;
    ERROR_CATCH(vkMapMemory(device, memory, allocation.offset , MIN(allocation.size, write_dscr->size), 0, &map_ptr) != VK_SUCCESS) {
        return FALSE;
    }
    memcpy(map_ptr, write_dscr->src, write_dscr->size);
    vkUnmapMemory(device, memory);
    return TRUE;
}

void vramGetMemoryDscr(u64 handle, VramMemoryDscr* const memory_dscr) {
    u32 block_id, alloc_id;
    UNPACK_MEMORY_HANDLE(handle, block_id, alloc_id)
    VramAllocation allocation = VRAM_ALLOCATION(s_vram_allocations, block_id, alloc_id);
    *memory_dscr = (VramMemoryDscr) {
        .memory = s_vram_blocks[block_id].memory,
        .offset = allocation.offset,
        .size = allocation.size
    };
}

void vramDebugPrintLayout(void) {
    printf("VRAM.C DEBUG LAYOUT:\n");
    for(u32 i = 0; i < MEMORY_BLOCK_COUNT; i++) {
        printf("\tblock_id: %u {\n", i);
        u32 alloc_count = VRAM_ALLOCATION_COUNT(s_vram_allocation_counts, i);
        printf("\t\tallocation_count: %u\n", alloc_count);
        for(u32 j = 0; j < MEMORY_BLOCK_MAX_ALLOCATIONS; j++) {
            VramAllocation allocation = VRAM_ALLOCATION(s_vram_allocations, i, j);
            if(!allocation.size && !allocation.offset) continue;
            printf("\t\t\tallocation_id: %u offset: %llu size: %llu\n", j, allocation.offset, allocation.size);
        }

        u32 free_count = VRAM_ALLOCATION_COUNT(s_vram_free_space_counts, i);
        printf("\t\tfree_space_count: %u\n", free_count);
        for(u32 j = 0; j < free_count; j++) {
            VramAllocation space = VRAM_ALLOCATION(s_vram_free_spaces, i, j);
            printf("\t\t\tfree_sapce_id: %u offset: %llu size: %llu\n", j, space.offset, space.size);
        }
        printf("\t}\n");
    }
}
