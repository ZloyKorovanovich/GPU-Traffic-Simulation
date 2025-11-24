
#define INCLUDE_VULKAN_INTERNAL
#define INCLUDE_QUEUE_RENDER
#include "vulkan.h"
#include "shaders.h"


typedef struct {
    VkImage image;
    VkImageView view;
    VkFormat format;
    u64 handle;
} DepthBuffer;

typedef enum {
    PIPELINE_DISTRIBUTION_ID = 0,
    PIPELINE_TRIANGLE_ID,
    PIPELINE_DEFAULT_MESH_ID,

    PIPELINE_COUNT
} PipelineIDs;

static void* s_pipeline_buffer = NULL; 
static VkPipeline* s_pipelines = NULL;
static VkPipelineLayout* s_pipeline_layouts = NULL;

static VkCommandPool s_command_pool = NULL;
static VkDescriptorPool s_descriptor_pool = NULL;

static void* s_descriptor_buffer = NULL;
static VkDescriptorSetLayout* s_descriptor_layout_sets = NULL;
static VkDescriptorSet* s_descriptor_sets = NULL;
static u32 s_descriptor_layout_set_count = 0;

static DepthBuffer s_depth_buffer = (DepthBuffer){0};

static EventCallback s_callback = NULL;

// ================================================== SCREEN BUFFERS
// =================================================================

u32 createDepthBuffer(VkDevice device) {
    VkExtent2D screen_extent = getSwapchainContextPtr()->descriptor.extent;
    s_depth_buffer.format = VK_FORMAT_D16_UNORM;
    VkImageCreateInfo image_info = (VkImageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = (VkExtent3D){
            .width = screen_extent.width, 
            .height = screen_extent.height, 
            .depth = 1
        },
        .format = s_depth_buffer.format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    ERROR_CATCH(vkCreateImage(device, &image_info, NULL, &s_depth_buffer.image) != VK_SUCCESS) {
        return VK_ERR_FAILED_TO_CREATE_DEPTH_IMAGE;
    }
    VkImageViewCreateInfo image_view_info = (VkImageViewCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .image = s_depth_buffer.image,
        .format = s_depth_buffer.format,
        .subresourceRange = (VkImageSubresourceRange) {
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
        }
    };

    ERROR_CATCH(vramAllocateImages(1, &s_depth_buffer.image, MEMORY_BLOCK_DEVICE_ID, &s_depth_buffer.handle) != VRAM_ALLOCATE_SUCCESS) {
        return VK_ERR_FAILED_TO_ALLOCATE_DEPTH_IMAGE;
    }
    ERROR_CATCH(vkCreateImageView(device, &image_view_info, NULL, &s_depth_buffer.view) != VK_SUCCESS) {
        return VK_ERR_FAILED_TO_CREATE_DEPTH_IMAGE_VIEW;
    }
    return CODE_SUCCESS;
}

void destroyDepthBuffer(VkDevice device) {
    vkDestroyImageView(device, s_depth_buffer.view, NULL);
    vkDestroyImage(device, s_depth_buffer.image, NULL);
    vramFree(s_depth_buffer.handle);
    s_depth_buffer = (DepthBuffer){0};
}

// ================================================ RENDER PIPELINES
// =================================================================

u32 createDistributionPipeline(VkDevice device, const VkShaderModule* shader_modules, VkPipeline* const pipeline, VkPipelineLayout* const layout) {
    VkPipelineLayoutCreateInfo layout_info = (VkPipelineLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .flags = 0,
        .pPushConstantRanges = NULL,
        .pushConstantRangeCount = 0,
        .setLayoutCount = 1,
        .pSetLayouts = s_descriptor_layout_sets
    };
    ERROR_CATCH(vkCreatePipelineLayout(device, &layout_info, NULL, layout) != VK_SUCCESS) {
        return VK_ERR_DISTRIBUTION_PIPELINE_LAYOUT_CREATE;
    }

    VkComputePipelineCreateInfo pipeline_info = (VkComputePipelineCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .basePipelineHandle = NULL,
        .basePipelineIndex = -1,
        .stage = (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_ENTRY_COMPUTE,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader_modules[SHADER_ID_DISTRIBUTION_C]
        },
        .layout = *layout
    };
    ERROR_CATCH(vkCreateComputePipelines(device, NULL, 1, &pipeline_info, NULL, pipeline) != VK_SUCCESS) {
        return VK_ERR_DISTRIBUTION_PIPELINE_CREATE;
    }
    return CODE_SUCCESS;
}

u32 createTrianglePipeline(VkDevice device, const VkShaderModule* shader_modules, VkPipeline* const pipeline, VkPipelineLayout* const layout) {
    VkPipelineLayoutCreateInfo layout_info = (VkPipelineLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .flags = 0,
        .pPushConstantRanges = NULL,
        .pushConstantRangeCount = 0,
        .setLayoutCount = 1,
        .pSetLayouts = s_descriptor_layout_sets
    };
    ERROR_CATCH(vkCreatePipelineLayout(device, &layout_info, NULL, layout) != VK_SUCCESS) {
        return VK_ERR_TRIANGLE_PIPELINE_LAYOUT_CREATE;
    }

    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_ENTRY_VERTEX,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = shader_modules[SHADER_ID_TRIANGLE_V]
        },
        (VkPipelineShaderStageCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pName = SHADER_ENTRY_FRAGMENT,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = shader_modules[SHADER_ID_TRIANGLE_F]
        }
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = (VkPipelineDynamicStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = (const VkDynamicState[]){VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_state = (VkPipelineVertexInputStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexAttributeDescriptionCount = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL,
        .pVertexBindingDescriptions = NULL
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = (VkPipelineInputAssemblyStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = FALSE
    };
    VkPipelineRasterizationStateCreateInfo rasterization_state = (VkPipelineRasterizationStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = FALSE,
        .rasterizerDiscardEnable = FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f
    };
    VkPipelineMultisampleStateCreateInfo multisample_state = (VkPipelineMultisampleStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = FALSE,
        .alphaToOneEnable = FALSE
    };
    VkPipelineColorBlendAttachmentState color_blend_attachment_state = (VkPipelineColorBlendAttachmentState) {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD
    };
    VkPipelineColorBlendStateCreateInfo color_blend_state = (VkPipelineColorBlendStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_state,
        .blendConstants[0] = 0.0f,
        .blendConstants[1] = 0.0f,
        .blendConstants[2] = 0.0f,
        .blendConstants[3] = 0.0f
    };
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = (VkPipelineDepthStencilStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = TRUE,
        .depthWriteEnable = TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    };
    VkPipelineRenderingCreateInfoKHR rendering_create_info = (VkPipelineRenderingCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &getSwapchainContextPtr()->descriptor.color_format.format,
        .depthAttachmentFormat = s_depth_buffer.format
    };

    VkPipelineViewportStateCreateInfo viewport_state = (VkPipelineViewportStateCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkGraphicsPipelineCreateInfo pipeline_info = (VkGraphicsPipelineCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .basePipelineHandle = NULL,
        .basePipelineIndex = -1,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &input_assembly_state,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization_state,
        .pMultisampleState = &multisample_state,
        .pDepthStencilState = &depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = &dynamic_state,
        .layout = *layout,
        .renderPass = NULL,
        .pNext = &rendering_create_info
    };

    ERROR_CATCH(vkCreateGraphicsPipelines(device, NULL, 1, &pipeline_info, NULL, pipeline) != VK_SUCCESS) {
        return VK_ERR_TRIANGLE_PIPELINE_CREATE;
    }
    return CODE_SUCCESS;
}

// ===================================================== DESCRIPTORS
// =================================================================

u32 createDescriptorSets(VkDevice device, VkDescriptorPool pool, VkDescriptorSet* const descriptor_sets, VkDescriptorSetLayout* const layout_sets) {
    VkDescriptorSetLayoutCreateInfo layout_set_info = (VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = (VkDescriptorSetLayoutBinding[]) {
            (VkDescriptorSetLayoutBinding) {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL
            },
            (VkDescriptorSetLayoutBinding) {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_ALL
            }
        }
    };
    ERROR_CATCH(vkCreateDescriptorSetLayout(device, &layout_set_info, NULL, layout_sets) != VK_SUCCESS) {
        return VK_ERR_CREATE_DESCRIPTOR_SET_LAYOUT;
    }

    VkDescriptorSetAllocateInfo allocate_info = (VkDescriptorSetAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = layout_sets
    };
    ERROR_CATCH(vkAllocateDescriptorSets(device, &allocate_info, descriptor_sets) != VK_SUCCESS) {
        return VK_ERR_ALLOCATE_DISCRIPTOR_SET;
    }
    return CODE_SUCCESS;
}

// ================================================ RENDER INTERFACE
// =================================================================

#define _INVOKE_CALLBACK(code)              \
return_code = MAKE_VK_ERR(code);            \
if(s_callback(return_code)) {               \
    goto _end;                              \
}
#define ERROR_CATCH_CALL(exp)               \
call_result = exp;                          \
ERROR_CATCH(call_result != CODE_SUCCESS) {  \
    _INVOKE_CALLBACK(call_result);          \
}

typedef struct {
    float viewport_params[4];
    float time;
    float delta;
} GlobalUniformBuffer;

#define UNIFORM_BUFFER_USED_SIZE (sizeof(GlobalUniformBuffer))
#define POSITION_BUFFER_USED_SIZE (sizeof(float) * 4 * 64)

/*
    all objects like image_available_semaphore, frame_fence etc should be one pre frame in-flight
    BUT! submit semaphores should be one per swapchain imaged and indexed with aquire image id
*/
typedef struct {
    VkSemaphore image_submit_semaphores[SWAPCHAIN_MAX_IMAGE_COUNT];
    VkSemaphore image_available_semaphore;
    VkCommandBuffer cmd_buffer;
    VkFence frame_fence;
    VkBufferMemoryBarrier position_buffer_barrier;
    VkImageMemoryBarrier image_top_barrier;
    VkImageMemoryBarrier image_bottom_barrier;
} RenderObjects;

typedef struct {
    u64 host_handle;
    u64 device_handle;
    VkBuffer host_uniform_buffer;
    VkBuffer device_uniform_buffer;
    VkBuffer device_position_buffer;
} RenderBuffers;

typedef struct {
    VkRenderingAttachmentInfoKHR screen_color;
    VkRenderingAttachmentInfoKHR screen_depth;
    VkRenderingInfoKHR screen_target;
} RenderTargets;

u32 createRenderObjects(VkDevice device, const RenderBuffers* render_buffers, RenderObjects* const render_objects) {
    VkCommandBufferAllocateInfo cmbuffers_info = (VkCommandBufferAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandBufferCount = 1,
        .commandPool = s_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    };
    ERROR_CATCH(vkAllocateCommandBuffers(device, &cmbuffers_info, &render_objects->cmd_buffer) != VK_SUCCESS) {
        return VK_ERR_COMMAND_BUFFER_ALLOCATE;
    }
    VkSemaphoreCreateInfo semaphore_info = (VkSemaphoreCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fence_info = (VkFenceCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    for(u32 i = 0; i < SWAPCHAIN_MAX_IMAGE_COUNT; i++) {
        ERROR_CATCH(vkCreateSemaphore(device, &semaphore_info, NULL, render_objects->image_submit_semaphores + i) != VK_SUCCESS) {
            return VK_ERR_SEAMAPHORE_CREATE;
        };
    }
    ERROR_CATCH(vkCreateSemaphore(device, &semaphore_info, NULL, &render_objects->image_available_semaphore) != VK_SUCCESS) {
        return VK_ERR_SEAMAPHORE_CREATE;
    };
    ERROR_CATCH(vkCreateFence(device, &fence_info, NULL, &render_objects->frame_fence)) {
        return VK_ERR_FENCE_CREATE;
    }

    render_objects->image_top_barrier = (VkImageMemoryBarrier) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
    render_objects->image_bottom_barrier = (VkImageMemoryBarrier) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
    render_objects->position_buffer_barrier = (VkBufferMemoryBarrier) {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .buffer = render_buffers->device_position_buffer,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .size = POSITION_BUFFER_USED_SIZE
    };
    return CODE_SUCCESS;
}

void destroyRenderObjects(VkDevice device, RenderObjects* const render_objects) {
    for(u32 i = 0; i < SWAPCHAIN_MAX_IMAGE_COUNT; i++) {
        SAFE_DESTROY(render_objects->image_submit_semaphores[i], vkDestroySemaphore(device, render_objects->image_submit_semaphores[i], NULL))
    }
    SAFE_DESTROY(render_objects->image_available_semaphore, vkDestroySemaphore(device, render_objects->image_available_semaphore, NULL))
    SAFE_DESTROY(render_objects->frame_fence, vkDestroyFence(device, render_objects->frame_fence, NULL))
    *render_objects = (RenderObjects){0};
}

u32 createRenderBuffers(VkDevice device, const QueueContext* queue_context, RenderBuffers* const render_buffers) {
    VkBufferCreateInfo buffer_create_info = (VkBufferCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queue_context->queue_locators[CURRENT_QUEUE_ID].family_id,
        .size = UNIFORM_BUFFER_USED_SIZE,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    ERROR_CATCH(vkCreateBuffer(device, &buffer_create_info, NULL, &render_buffers->device_uniform_buffer) != VK_SUCCESS) {
        return VK_ERR_GLOBAL_UNIFORM_BUFFER_DEVICE_CREATE;
    }
    buffer_create_info = (VkBufferCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queue_context->queue_locators[CURRENT_QUEUE_ID].family_id,
        .size = UNIFORM_BUFFER_USED_SIZE,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    ERROR_CATCH(vkCreateBuffer(device, &buffer_create_info, NULL, &render_buffers->host_uniform_buffer) != VK_SUCCESS) {
        return VK_ERR_GLOBAL_UNIFORM_BUFFER_HOST_CREATE;
    }
    buffer_create_info = (VkBufferCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queue_context->queue_locators[CURRENT_QUEUE_ID].family_id,
        .size = POSITION_BUFFER_USED_SIZE,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    ERROR_CATCH(vkCreateBuffer(device, &buffer_create_info, NULL, &render_buffers->device_position_buffer) != VK_SUCCESS) {
        return VK_ERR_POSITION_BUFFER_HOST_CREATE;
    }

    ERROR_CATCH(vramAllocateBuffers(1, &render_buffers->host_uniform_buffer, MEMORY_BLOCK_HOST_ID, &render_buffers->host_handle) != VRAM_ALLOCATE_SUCCESS) {
        return VK_ERR_GLOBAL_UNIFORM_BUFFER_HOST_ALLOCATE;
    }
    ERROR_CATCH(vramAllocateBuffers(2, (VkBuffer[]){render_buffers->device_position_buffer, render_buffers->device_uniform_buffer}, MEMORY_BLOCK_DEVICE_ID, &render_buffers->device_handle) != VRAM_ALLOCATE_SUCCESS) {
        return VK_ERR_GLOBAL_UNIFORM_BUFFER_DEVICE_ALLOCATE;
    }
    return CODE_SUCCESS;
}

void destroyRenderBuffers(VkDevice device, RenderBuffers* const render_buffers) {
    SAFE_DESTROY(render_buffers->host_uniform_buffer, vkDestroyBuffer(device, render_buffers->host_uniform_buffer, NULL))
    SAFE_DESTROY(render_buffers->device_uniform_buffer, vkDestroyBuffer(device, render_buffers->device_uniform_buffer, NULL))
    SAFE_DESTROY(render_buffers->device_position_buffer, vkDestroyBuffer(device, render_buffers->device_position_buffer, NULL))
    SAFE_DESTROY(render_buffers->host_handle, vramFree(render_buffers->host_handle))
    SAFE_DESTROY(render_buffers->device_handle, vramFree(render_buffers->device_handle))
}


result defaultUpdate(f64 time, f64 delta) {
    return TRUE;
}

void renderLoopExit(void) {
    glfwSetWindowShouldClose(getVulkanContextPtr()->window, TRUE);
}

result renderLoop(UpdateCallback update_callback) {
    update_callback = update_callback ? update_callback : &defaultUpdate;
    result return_code = CODE_SUCCESS;

    VulkanContext vulkan_context = *getVulkanContextPtr();
    QueueContext queue_context = *getQueueContextPtr();
    ExtContext ext_context = *getExtensionContextPtr();

    double time;
    double delta_time;
    VkViewport viewport;
    u32 image_id;
    u32 call_result;

    RenderBuffers render_buffers;
    RenderObjects render_objects;
    ERROR_CATCH_CALL(
        createRenderBuffers(vulkan_context.device, &queue_context, &render_buffers)
    )
    ERROR_CATCH_CALL(
        createRenderObjects(vulkan_context.device, &render_buffers, &render_objects)
    )

// ===================================================== RENDER INFO
    RenderTargets render_targets = (RenderTargets) {
        .screen_color = (VkRenderingAttachmentInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE
        },
        .screen_depth = (VkRenderingAttachmentInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .clearValue = (VkClearValue) {
                .depthStencil = (VkClearDepthStencilValue){
                    .depth = 1.0
                }
            },
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE
        },
        .screen_target = (VkRenderingInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            .colorAttachmentCount = 1,
            .pColorAttachments = &render_targets.screen_color,
            .layerCount = 1,
            .pDepthAttachment = &render_targets.screen_depth
        }
    };
    

// ================================================= DESCRIPTOR SETS
    VkDescriptorBufferInfo descriptor_uniform_buffer_info = (VkDescriptorBufferInfo) {
        .buffer = render_buffers.device_uniform_buffer,
        .offset = 0,
        .range = UNIFORM_BUFFER_USED_SIZE
    };
    VkDescriptorBufferInfo descriptor_position_buffer_info = (VkDescriptorBufferInfo) {
        .buffer = render_buffers.device_position_buffer,
        .offset = 0,
        .range = POSITION_BUFFER_USED_SIZE
    };
    VkWriteDescriptorSet descriptor_set_writes[] = {
        (VkWriteDescriptorSet) {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = s_descriptor_sets[0],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .pBufferInfo = &descriptor_uniform_buffer_info,
            .pImageInfo = NULL,
            .pTexelBufferView = NULL
        },
        (VkWriteDescriptorSet) {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = s_descriptor_sets[0],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .pBufferInfo = &descriptor_position_buffer_info,
            .pImageInfo = NULL,
            .pTexelBufferView = NULL
        }
    };
    vkUpdateDescriptorSets(vulkan_context.device, 2, descriptor_set_writes, 0, NULL);

// =================================================== TIME COUNTERS
#ifdef _WIN32
    LARGE_INTEGER win_last_time;
    LARGE_INTEGER win_current_time;
    LARGE_INTEGER win_cpu_frequency;
    QueryPerformanceCounter(&win_last_time);
#endif
    time = 0;
    delta_time = 0;

// @(Mitro): was tested on debian kali linux, and might be redundant
#ifdef linux
#define _WINDOW_MIN_SIZE 16
    int last_width, last_height;
    int current_width, current_height;
    glfwGetFramebufferSize(vulkan_context.window, &current_width, &current_height);
    last_width = MAX(_WINDOW_MIN_SIZE, current_width);
    last_height = MAX(_WINDOW_MIN_SIZE, current_height);
#endif // defined(linux)


// =================================================== RENDER_LOOP
    while(!glfwWindowShouldClose(vulkan_context.window)) {
// =============================================================== FRAME START
        glfwPollEvents();
// =============================================================== TIME COUNTERS
#ifdef _WIN32
        QueryPerformanceCounter(&win_current_time);
        QueryPerformanceFrequency(&win_cpu_frequency);
        delta_time = (double)(win_current_time.QuadPart - win_last_time.QuadPart) / (double)win_cpu_frequency.QuadPart;
        time += delta_time;
        win_last_time = win_current_time;
#endif
        update_callback(time, delta_time);
// =============================================================== IMAGE ACQUIRE
        vkWaitForFences(vulkan_context.device, 1, &render_objects.frame_fence, VK_TRUE, U64_MAX);
        VkResult image_acquire_result = vkAcquireNextImageKHR(vulkan_context.device, getSwapchainContextPtr()->swapchain, U64_MAX, render_objects.image_available_semaphore, NULL, &image_id);
        
#ifdef linux
        glfwGetFramebufferSize(vulkan_context.window, &current_width, &current_height);
        if((current_width > _WINDOW_MIN_SIZE && current_width != last_width) || (current_height > _WINDOW_MIN_SIZE && current_height != last_height)) {
            vkDeviceWaitIdle(vulkan_context.device);
            recreateSwapchain();
            destroyDepthBuffer(vulkan_context.device);
            createDepthBuffer(vulkan_context.device);
            last_width = MAX(_WINDOW_MIN_SIZE, current_width);
            last_height = MAX(_WINDOW_MIN_SIZE, current_height);
            continue;
        }
#else // not defined(linux)
        //@(Mitro): check VK_SUBOPTIMAL_KHR check
        if(image_acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
            vkDeviceWaitIdle(vulkan_context.device);
            recreateSwapchain();
            destroyDepthBuffer(vulkan_context.device);
            createDepthBuffer(vulkan_context.device);
            continue;
        }
#endif
// =============================================================== SETUP DATA FOR FRAME
        render_targets.screen_color.imageView = getSwapchainContextPtr()->views[image_id];
        render_targets.screen_depth.imageView = s_depth_buffer.view; // always the same
        render_targets.screen_target.renderArea = (VkRect2D) {
            .extent = getSwapchainContextPtr()->descriptor.extent,
            .offset = (VkOffset2D){0}
        };
        viewport = (VkViewport) {
            .width = (float)render_targets.screen_target.renderArea.extent.width,
            .height = (float)render_targets.screen_target.renderArea.extent.height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0,
            .y = 0
        };
        render_objects.image_top_barrier.image = getSwapchainContextPtr()->images[image_id];
        render_objects.image_bottom_barrier.image = getSwapchainContextPtr()->images[image_id];

        GlobalUniformBuffer gubuffer_struct = (GlobalUniformBuffer) {
            .viewport_params = {viewport.x, viewport.y, viewport.width, viewport.height},
            .time = (float)time,
            .delta = (float)delta_time
        };

        ERROR_CATCH(!vramWriteToAllocation(render_buffers.host_handle, (VramWriteDscr[]){{.src = &gubuffer_struct, .size = sizeof(GlobalUniformBuffer)}})) {
            _INVOKE_CALLBACK(VK_ERR_GLOBAL_UNIFORM_BUFFER_WRITE)
        }
        
// =============================================================== COMMAND BUFFER BEGIN
        VkCommandBufferBeginInfo command_buffer_begin_info = (VkCommandBufferBeginInfo) {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = 0,
            .pInheritanceInfo = NULL
        };

        vkResetFences(vulkan_context.device, 1, &render_objects.frame_fence);
        vkResetCommandBuffer(render_objects.cmd_buffer, 0);
        vkBeginCommandBuffer(render_objects.cmd_buffer, &command_buffer_begin_info);//@(Mitro): should chekc error if fails

// =============================================================== TRANSFER BUFFERS
// @(Mitro): generaly thats a bad place for that, as later, when data becomes bigger we plan on using async transfer queue

        VkBufferCopy buffer_copy = (VkBufferCopy) {
            .dstOffset = 0,
            .srcOffset = 0,
            .size = sizeof(GlobalUniformBuffer)
        };
        vkCmdCopyBuffer(render_objects.cmd_buffer, render_buffers.host_uniform_buffer, render_buffers.device_uniform_buffer, 1, &buffer_copy);

        vkCmdPipelineBarrier(
            render_objects.cmd_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
            0, 0, NULL, 0, NULL, 0, NULL
        );

// =============================================================== DISTRIBUTION PASS
        vkCmdBindDescriptorSets(render_objects.cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, s_pipeline_layouts[PIPELINE_DISTRIBUTION_ID], 0, 1, s_descriptor_sets, 0, NULL);
        vkCmdBindDescriptorSets(render_objects.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipeline_layouts[PIPELINE_TRIANGLE_ID], 0, 1, s_descriptor_sets, 0, NULL);

        vkCmdBindPipeline(render_objects.cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, s_pipelines[PIPELINE_DISTRIBUTION_ID]);
        vkCmdDispatch(render_objects.cmd_buffer, 1, 1, 1);
        

        vkCmdPipelineBarrier(
            render_objects.cmd_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
            0, 0, NULL, 1, &render_objects.position_buffer_barrier, 0, NULL
        );
        vkCmdPipelineBarrier(
            render_objects.cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 
            0, 0, NULL, 0, NULL, 1, &render_objects.image_top_barrier
        );

// =============================================================== RENDERING ON SCREEN
        ext_context.cmd_begin_rendering(render_objects.cmd_buffer, &render_targets.screen_target);
        
        vkCmdBindPipeline(render_objects.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_pipelines[PIPELINE_TRIANGLE_ID]);
        vkCmdSetViewport(render_objects.cmd_buffer, 0, 1, &viewport);
        vkCmdSetScissor(render_objects.cmd_buffer, 0, 1, &render_targets.screen_target.renderArea);

        vkCmdDraw(render_objects.cmd_buffer, 3 * 64, 1, 0, 0);

        ext_context.cmd_end_rendering(render_objects.cmd_buffer); 
        vkCmdPipelineBarrier(
            render_objects.cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
            0, 0, NULL, 0, NULL, 1, &render_objects.image_bottom_barrier
        );

// =============================================================== END COMMAND BUFFER & SUBMIT
        vkEndCommandBuffer(render_objects.cmd_buffer);
        VkSubmitInfo submit_info = (VkSubmitInfo) {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_objects.image_available_semaphore,
            .pWaitDstStageMask = (const VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_objects.image_submit_semaphores[image_id],
            .commandBufferCount = 1,
            .pCommandBuffers = &render_objects.cmd_buffer
        };
        ERROR_CATCH(vkQueueSubmit(queue_context.queues[CURRENT_QUEUE_ID], 1, &submit_info, render_objects.frame_fence) != VK_SUCCESS) {
            _INVOKE_CALLBACK(VK_ERR_QUEUE_SUBMIT)
        }

        VkPresentInfoKHR present_info = (VkPresentInfoKHR) {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_objects.image_submit_semaphores[image_id],
            .swapchainCount = 1,
            .pSwapchains = &getSwapchainContextPtr()->swapchain,
            .pImageIndices = &image_id,
            .pResults = NULL
        };
        VkResult present_result = vkQueuePresentKHR(queue_context.queues[CURRENT_QUEUE_ID], &present_info);
        if(present_result == VK_ERROR_OUT_OF_DATE_KHR) {
            vkDeviceWaitIdle(vulkan_context.device);
            recreateSwapchain();
            destroyDepthBuffer(vulkan_context.device);
            createDepthBuffer(vulkan_context.device);
            continue;
        }
    }
    vkDeviceWaitIdle(vulkan_context.device);
_end:
    destroyRenderObjects(vulkan_context.device, &render_objects);
    destroyRenderBuffers(vulkan_context.device, &render_buffers);
    return return_code;
}

result renderInit(EventCallback event_callback) {
    u32 call_result;
    result return_code = CODE_SUCCESS;
    VulkanContext vulkan_context = *getVulkanContextPtr();
    QueueContext queue_context = *getQueueContextPtr();
    s_callback = event_callback ? event_callback : vulkan_context.callback;

    // creating allocation pools
    VkCommandPoolCreateInfo command_pool_info = (VkCommandPoolCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queue_context.queue_locators[CURRENT_QUEUE_ID].family_id,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    ERROR_CATCH(vkCreateCommandPool(vulkan_context.device, &command_pool_info, NULL, &s_command_pool) != VK_SUCCESS) {
        _INVOKE_CALLBACK(VK_ERR_COMMAND_POOL_CREATE)
    }
        
    VkDescriptorPoolCreateInfo descriptor_pool_info = (VkDescriptorPoolCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = DESCRIPTOR_SET_COUNT,
        .poolSizeCount = 2,
        .pPoolSizes = (VkDescriptorPoolSize[]) {
            (VkDescriptorPoolSize) {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = DESCRIPTOR_MAX_COUNT
            },
            (VkDescriptorPoolSize) {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = DESCRIPTOR_MAX_COUNT
            }
        }
    };
    ERROR_CATCH(vkCreateDescriptorPool(vulkan_context.device, &descriptor_pool_info, NULL, &s_descriptor_pool) != VK_SUCCESS) {
        _INVOKE_CALLBACK(VK_ERR_DESCRIPTOR_POOL_CREATE)
    }

    // descriptor set creation
    s_descriptor_buffer = malloc(sizeof(void*) * DESCRIPTOR_SET_COUNT * 2);
    ERROR_CATCH(!s_descriptor_buffer) {
        _INVOKE_CALLBACK(VK_ERR_DESCRIPTOR_BUFFER_ALLOCATE)
    }

    s_descriptor_layout_sets = s_descriptor_buffer;
    s_descriptor_sets = (VkDescriptorSet*)((void**)s_descriptor_buffer + DESCRIPTOR_SET_COUNT);
    s_descriptor_layout_set_count = 1;
    ERROR_CATCH_CALL(
        createDescriptorSets(vulkan_context.device, s_descriptor_pool, s_descriptor_sets, s_descriptor_layout_sets)
    )

    // depth buffer
    ERROR_CATCH_CALL(
        createDepthBuffer(vulkan_context.device)
    )

    // pipelines creation
    s_pipeline_buffer = malloc(sizeof(void*) * PIPELINE_COUNT * 2);
    ERROR_CATCH(!s_pipeline_buffer) {
        _INVOKE_CALLBACK(VK_ERR_PIPELINE_BUFFER_ALLOCATE)
    }
    s_pipeline_layouts = s_pipeline_buffer;
    s_pipelines = (VkPipeline*)((void**)s_pipeline_buffer + PIPELINE_COUNT);

    const VkShaderModule* shader_modules = getShaderModulesPtr();
    ERROR_CATCH_CALL(
        createDistributionPipeline(vulkan_context.device, shader_modules, s_pipelines + PIPELINE_DISTRIBUTION_ID, s_pipeline_layouts + PIPELINE_DISTRIBUTION_ID)
    )
    ERROR_CATCH_CALL(
        createTrianglePipeline(vulkan_context.device, shader_modules, s_pipelines + PIPELINE_TRIANGLE_ID, s_pipeline_layouts + PIPELINE_TRIANGLE_ID)
    )

_end:
    return return_code;
}

void renderTerminate(void) {
    VulkanContext vulkan_context = *getVulkanContextPtr();

    for(u32 i = 0; i < PIPELINE_COUNT; i++) {
        SAFE_DESTROY(s_pipelines[i], vkDestroyPipeline(vulkan_context.device, s_pipelines[i], NULL))
        SAFE_DESTROY(s_pipeline_layouts[i], vkDestroyPipelineLayout(vulkan_context.device, s_pipeline_layouts[i], NULL))
    }
    SAFE_DESTROY(s_pipeline_buffer, free(s_pipeline_buffer))
    s_pipeline_layouts = NULL;
    s_pipelines = NULL;

    destroyDepthBuffer(vulkan_context.device);

    for(u32 i = 0; i < s_descriptor_layout_set_count; i++) {
        SAFE_DESTROY(s_descriptor_layout_sets[i], vkDestroyDescriptorSetLayout(vulkan_context.device, s_descriptor_layout_sets[i], NULL))
    }
    SAFE_DESTROY(s_descriptor_buffer, free(s_descriptor_buffer));
    s_descriptor_layout_sets = NULL;
    s_descriptor_sets = NULL;
    s_descriptor_layout_set_count = 0;
    SAFE_DESTROY(s_descriptor_pool, vkDestroyDescriptorPool(vulkan_context.device, s_descriptor_pool, NULL))
    SAFE_DESTROY(s_command_pool, vkDestroyCommandPool(vulkan_context.device, s_command_pool, NULL))
}
