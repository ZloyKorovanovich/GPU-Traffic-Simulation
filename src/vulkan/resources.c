#define INCLUDE_VULKAN_INTERNAL
#include "vulkan.h"
#define INCLUDE_SHADERS_INETERNAL
#include "shaders.h"


static VkShaderModule* s_shader_modules = NULL;
static EventCallback s_callback = NULL;

b32 readShaderFile(const char* path, u64 buffer_size, char* const buffer) {
    FILE* file = fopen(path, "rb");
    ERROR_CATCH(!file) return FALSE;
    for(u32 i = 0; i < buffer_size; i++) {
        buffer[i] = getc(file);
    }
    fclose(file);
    return TRUE;
}

#define _INVOKE_CALLBACK(code)                                                  \
return_code = MAKE_VK_ERR(code);                                                \
if(s_callback(return_code)) {                                                   \
    goto _end;                                                                  \
}

result resourcesInit(const char* res_path) {
    VulkanContext vulkan_context = *getVulkanContextPtr();
    result return_code = CODE_SUCCESS;
    s_callback = vulkan_context.callback;
    // path generation
    char shader_path[256];
    strcpy(shader_path, res_path);
    strcat(shader_path, "shaders.res");


    // read shaders and create shader objects
    void* shader_buffer = malloc(SHADER_BUFFER_SIZE);
    ERROR_CATCH(!shader_buffer) {
        _INVOKE_CALLBACK(VK_ERR_SHADER_BUFFER_ALLOCATE);
    }
    ERROR_CATCH(!readShaderFile(shader_path, SHADER_BUFFER_SIZE, shader_buffer)) {
        _INVOKE_CALLBACK(VK_ERR_SHADER_BUFFER_LOAD)
    }

    s_shader_modules = malloc(sizeof(VkShaderModule) * SHADER_COUNT);
    ERROR_CATCH(!s_shader_modules) {
        _INVOKE_CALLBACK(VK_ERR_SHADER_MODULE_ARRAY_ALLOCATE);
    }
    VkShaderModuleCreateInfo shader_module_info = (VkShaderModuleCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    };
    for(u32 i = 0; i < SHADER_COUNT; i++) {
        shader_module_info.pCode = (u32*)((char*)shader_buffer + c_shader_infos[i].code_offset);
        shader_module_info.codeSize = c_shader_infos[i].code_size;
        vkCreateShaderModule(vulkan_context.device, &shader_module_info, NULL, s_shader_modules + i);
    }
_end:
    SAFE_DESTROY(shader_buffer, free(shader_buffer))
    return return_code;
}

void resourcesTerminate(void) {
    VulkanContext vulkan_context = *getVulkanContextPtr();

    for(u32 i = 0; i < SHADER_COUNT; i++) {
        SAFE_DESTROY(s_shader_modules[i], vkDestroyShaderModule(vulkan_context.device, s_shader_modules[i], NULL))
    }
    SAFE_DESTROY(s_shader_modules, free(s_shader_modules));
    s_callback = NULL;
}

// ========================================================= CONTEXT
// =================================================================

const VkShaderModule* getShaderModulesPtr(void) {
    return s_shader_modules;
}