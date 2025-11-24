#define INCLUDE_VULKAN_INTERNAL
#include "vulkan.h"

const u32 c_vulkan_layer_count = 1;
const char* c_vulkan_layers[] = {"VK_LAYER_KHRONOS_validation"};
const VkDebugUtilsMessageSeverityFlagsEXT c_debug_message_severity = (
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
);
const VkDebugUtilsMessageTypeFlagBitsEXT c_debug_message_type = (
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
);

const u32 c_vulkan_extension_count = INSTANCE_EXTENSION_COUNT;
const u32 c_vulkan_extension_count_debug = INSTANCE_EXTENSION_DEBUG_COUNT;
const char* c_vulkan_extensions[] = INSTANCE_EXTENSIONS;

const u32 c_queue_count = DEVICE_QUEUE_COUNT;
const u32 c_queue_flags[] = DEVICE_QUEUE_FLAGS;

const u32 c_device_extension_count = DEVICE_EXTENSION_COUNT;
const char* c_device_extensions[] = DEVICE_EXTENSIONS;

const u32 c_memory_block_count = MEMORY_BLOCK_COUNT;
const MemoryBlockDscr c_memory_block_descriptors[] = MEMORY_BLOCK_DESCRIPTORS;

const VkPhysicalDeviceFeatures c_device_features = DEVICE_FEATURES;

static VkDebugUtilsMessengerEXT s_debug_messenger = NULL; // active only if initialized with debug flag
static PFN_vkCreateDebugUtilsMessengerEXT ext_create_debug_messenger = NULL;
static PFN_vkDestroyDebugUtilsMessengerEXT ext_destroy_debug_messenger = NULL;

static VulkanContext s_vulkan_context = (VulkanContext){0};
static QueueContext s_queue_context = (QueueContext){0};
static ExtContext s_ext_context = (ExtContext){0};
static SwapchainContext s_swapchain_context = (SwapchainContext){0};

b32 defaultCallback(u32 code) {
    return (code != CODE_SUCCESS);
}

// ============================================================ GLFW
// =================================================================

GLFWwindow* createWindow(u32 width, u32 height, u32 flags, const char* name) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, FLAG_IN_MASK(flags, VULKAN_FLAG_RESIZABLE));
    GLFWmonitor* active_monitor = FLAG_IN_MASK(flags, VULKAN_FLAG_FULLSCREEN)? glfwGetPrimaryMonitor() : NULL;
    return glfwCreateWindow((i32)width, (i32)height, name, active_monitor, NULL);
}

// =============================================== INSTANCE CREATION
// =================================================================

// callback for validation layers
VKAPI_ATTR VkBool32 VKAPI_CALL validationDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data
) {
    printf("%s\n", callback_data->pMessage);
    return !FLAG_IN_MASK(message_severity, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
}

b32 checkVulkanLayerSupport(u32 layer_count, const char** layer_names) {
    u32 vk_layer_count;
    vkEnumerateInstanceLayerProperties(&vk_layer_count, NULL);
    VkLayerProperties* vk_layers = ZE_ALLOCA(sizeof(VkLayerProperties) * vk_layer_count);
    vkEnumerateInstanceLayerProperties(&vk_layer_count, vk_layers);
    
    u32 found_layers = 0;
    for(u32 i = 0; i < vk_layer_count; i++) {
        const char* layer_name = vk_layers[i].layerName;
        for(u32 j = 0; j < layer_count; j++) {
            if(!strcmp(layer_name, layer_names[j])) {
                if(++found_layers == layer_count) return TRUE;
                break;
            }
        }
    }
    return FALSE;
}

b32 checkVulkanExtensionsSupport(u32 extension_count, const char** extension_names) {
    u32 vk_extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &vk_extension_count, NULL);
    VkExtensionProperties* vk_extensions = ZE_ALLOCA(sizeof(VkExtensionProperties) * vk_extension_count);
    vkEnumerateInstanceExtensionProperties(NULL, &vk_extension_count, vk_extensions);

    u32 found_extensions = 0;
    for(u32 i = 0; i < vk_extension_count; i++) {
        const char* layer_name = vk_extensions[i].extensionName;
        for(u32 j = 0; j < extension_count; j++) {
            if(!strcmp(layer_name, extension_names[j])) {
                if(++found_extensions == extension_count) return TRUE;
                break;
            }
        }
    }
    return FALSE;
}

// ================================================ DEVICE SELECTION
// =================================================================

/*
void debugPhysicalDevice(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    printf("Physical device: %s\n", properties.deviceName);
}
*/

b32 layoutDeviceQueues(VkPhysicalDevice device, u32 queue_count, const u32* queue_flags, QueueLocator* const queue_ids) {
    u32 queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = ZE_ALLOCA(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);
    u32* occupied_queues = ZE_ALLOCA(sizeof(u32) * queue_family_count);
    memset(occupied_queues, 0, sizeof(u32) * queue_family_count);

    u32 found_queues = 0;
    for(u32 i = 0; i < queue_count; i++) {
        u32 flags = queue_flags[i];
        for(u32 j = 0; j < queue_family_count; j++) {
            if(flags == (queue_families[j].queueFlags & DEVICE_QUEUE_FLAG_MASK) && queue_families[j].queueCount > occupied_queues[j]) {
                found_queues++;
                if(queue_ids) {
                    queue_ids[i] = (QueueLocator) {
                        .family_id = j,
                        .local_id = occupied_queues[j]
                    };
                }
                occupied_queues[j]++;
                if(found_queues == queue_count) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

b32 checkDeviceExtensions(VkPhysicalDevice device, u32 extension_count, const char** extension_names) {
    u32 device_extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &device_extension_count, NULL);
    VkExtensionProperties* device_extensions = ZE_ALLOCA(sizeof(VkExtensionProperties) * device_extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &device_extension_count, device_extensions);
    u32 found_extensions = 0;
    for(u32 i = 0; i < device_extension_count; i++) {
        const char* layer_name = device_extensions[i].extensionName;
        for(u32 j = 0; j < extension_count; j++) {
            if(!strcmp(layer_name, extension_names[j])) {
                if(++found_extensions == extension_count) return TRUE;
                break;
            }
        }
    }
    return FALSE;
}

b32 checkDeviceFeatures(VkPhysicalDevice device, const VkPhysicalDeviceFeatures* features) {
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);
    //@(Mitro): I know I break strict aliasing rules, but there is no other smart choise here
    for(u32 i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); i++) {
        if(*(b32*)(&features) && ! *(b32*)(&device_features)) {
            return FALSE;
        }
    }
    return TRUE;
}

b32 checkDeviceMemory(VkPhysicalDevice device, u32 block_count, const MemoryBlockDscr* block_dscrs) {
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
                goto _found;
            }
        }
//_not_found:
        return FALSE;
_found:
    }
    return TRUE;
}

u32 rateDeviceScore(VkPhysicalDevice device, u32* const device_id) {
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);

    if(device_id) {
        *device_id = device_properties.deviceID;
    }

    u32 score = 0;
    score += (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) * 1000;
    return score;
}

// you need to specify  per-family info for VkDeviceQueueCreateInfo, not per-queue info
void combineQueuesToFamilies(u32 queue_count, const QueueLocator* queues, u32* const family_count, QueueLocator* const families) {
    u32 family_number = 0;
    for(u32 i = 0; i < queue_count; i++) {
        for(u32 j = 0; j < family_number; j++) {
            if(families[j].family_id == queues[i].family_id) {
                families[j].local_id++;
                goto _found;
            }
        }
        families[family_number] = (QueueLocator) {
            .family_id = queues[i].family_id,
            .local_id = 1
        };
        family_number++;
_found:
    }
    *family_count = family_number;
}

// ======================================================= SWAPCHAIN
// =================================================================

void getScreenDescriptor(GLFWwindow* window, VkSurfaceKHR surface, VkPhysicalDevice device, SwapchainDscr* const dscr) {
    i32 width, height;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
    dscr->extent = (VkExtent2D){
        .width = CLAMP((u32)width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        .height = CLAMP((u32)height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
    dscr->surface_transform = capabilities.currentTransform;
    dscr->min_image_count = MAX(2, capabilities.minImageCount);

    u32 surface_format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_format_count, NULL);
    VkSurfaceFormatKHR* surface_formats = alloca(sizeof(VkSurfaceFormatKHR) * surface_format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_format_count, surface_formats);
    dscr->color_format = surface_formats[0];
    for(u32 i = 0; i < surface_format_count; i++) {
        if(surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            dscr->color_format = surface_formats[i];
            break;
        }
    }
    
    u32 present_modes_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes_count, NULL);
    VkPresentModeKHR* present_modes = alloca(sizeof(VkPresentModeKHR) * present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes_count, present_modes);
    dscr->present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for(u32 i = 0; i < present_modes_count; i++) {
        if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            dscr->present_mode = present_modes[i];
        }
    }
}

// ================================================ RENDER INTERFACE
// =================================================================


#define _INVOKE_CALLBACK(code)                                                  \
return_code = MAKE_VK_ERR(code);                                                \
if(s_vulkan_context.callback(return_code)) {                                    \
    goto _end;                                                                  \
}
#define _LOAD_EXT_FUNC(pfn, name)                                               \
pfn = (void*)vkGetInstanceProcAddr(s_vulkan_context.instance, #name);           \
if(!pfn) {                                                                      \
    _INVOKE_CALLBACK(VK_ERR_LOAD_EXT_PFN)                                       \
}

result coreInit(u32 width, u32 height, u32 flags, EventCallback callback) {
    result return_code = CODE_SUCCESS;
    // necessary to do the work and handle VK_ERRs
    s_vulkan_context.callback = callback ? callback : &defaultCallback;
    
    // creating window
    ERROR_CATCH(!glfwInit()) {
        _INVOKE_CALLBACK(VK_ERR_GLFW_INIT)
    }
    s_vulkan_context.window = createWindow(width, height, flags, "vulkan_window");
    ERROR_CATCH(!s_vulkan_context.window) {
        _INVOKE_CALLBACK(VK_ERR_WINDOW_CREATE)
    }

    // checking layers and extensions support and loading them 
    VkApplicationInfo app_info = (VkApplicationInfo) {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Insurgent",
        .pEngineName = "Insurgent_Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    u32 glfw_extension_count;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    const char** instance_extensions = ZE_ALLOCA(sizeof(void*) * c_vulkan_extension_count_debug); // debug count is always bigger than usual
    for(u32 i = 0; i < glfw_extension_count; i++) {
        instance_extensions[i] = glfw_extensions[i];
    }
    for(u32 i = 0; i < c_vulkan_extension_count_debug; i++) {
        instance_extensions[glfw_extension_count + i] = c_vulkan_extensions[i];
    }

    if(FLAG_IN_MASK(flags, VULKAN_FLAG_DEBUG)) {        
        u32 debug_extension_count = glfw_extension_count + c_vulkan_extension_count_debug; 

        ERROR_CATCH(!checkVulkanLayerSupport(c_vulkan_layer_count, c_vulkan_layers)) {
            _INVOKE_CALLBACK(VK_ERR_DEBUG_LAYERS_NOT_PRESENT)
        }
        ERROR_CATCH(!checkVulkanExtensionsSupport(debug_extension_count, instance_extensions)) {
            _INVOKE_CALLBACK(VK_ERR_EXT_NOT_PRESENT)
        }
        VkDebugUtilsMessengerCreateInfoEXT debug_info = (VkDebugUtilsMessengerCreateInfoEXT) {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = c_debug_message_severity,
            .messageType = c_debug_message_type,
            .pfnUserCallback = validationDebugCallback
        };
        VkInstanceCreateInfo instance_info = (VkInstanceCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app_info,
            .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
            .enabledExtensionCount = debug_extension_count,
            .ppEnabledExtensionNames = instance_extensions,
            .enabledLayerCount = c_vulkan_layer_count,
            .ppEnabledLayerNames = c_vulkan_layers,
            .pNext = &debug_info
        };
        ERROR_CATCH(vkCreateInstance(&instance_info, NULL, &s_vulkan_context.instance) != VK_SUCCESS) {
            _INVOKE_CALLBACK(VK_ERR_INSTANCE_CREATE)
        }

        _LOAD_EXT_FUNC(ext_create_debug_messenger, vkCreateDebugUtilsMessengerEXT)
        _LOAD_EXT_FUNC(ext_destroy_debug_messenger, vkDestroyDebugUtilsMessengerEXT)

        ERROR_CATCH(ext_create_debug_messenger(s_vulkan_context.instance, &debug_info, NULL, &s_debug_messenger) != VK_SUCCESS) {
            _INVOKE_CALLBACK(VK_ERR_DEBUG_MESSENGER_CREATE)
        }
    } else {
        u32 extension_count = glfw_extension_count + c_vulkan_extension_count;
        ERROR_CATCH(!checkVulkanExtensionsSupport(extension_count, instance_extensions)) {
            _INVOKE_CALLBACK(VK_ERR_EXT_NOT_PRESENT)
        }
        VkInstanceCreateInfo instance_info = (VkInstanceCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app_info,
            .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
            .enabledExtensionCount = extension_count,
            .ppEnabledExtensionNames = instance_extensions,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = NULL,
            .pNext = NULL
        };
        ERROR_CATCH(vkCreateInstance(&instance_info, NULL, &s_vulkan_context.instance) != VK_SUCCESS) {
            _INVOKE_CALLBACK(VK_ERR_INSTANCE_CREATE)
        }
    }

    ERROR_CATCH(glfwCreateWindowSurface(s_vulkan_context.instance, s_vulkan_context.window, NULL, &s_vulkan_context.surface) != VK_SUCCESS) {
        _INVOKE_CALLBACK(VK_ERR_SURFACE_CREATE)
    }

    // device selection
    u32 vk_device_count;
    vkEnumeratePhysicalDevices(s_vulkan_context.instance, &vk_device_count, NULL);
    ERROR_CATCH(vk_device_count == 0) {
        _INVOKE_CALLBACK(VK_ERR_NO_GPU)
    }
    VkPhysicalDevice* vk_devices = alloca(sizeof(VkPhysicalDevice) * vk_device_count);
    vkEnumeratePhysicalDevices(s_vulkan_context.instance, &vk_device_count, vk_devices);

    // bootstrap in unknown environment
    s_vulkan_context.physical_device = NULL;
    u32 best_score = 0;
    for(u32 i = 0; i < vk_device_count; i++) {
        VkPhysicalDevice vk_physical_device = vk_devices[i];
        if(!layoutDeviceQueues(vk_physical_device, c_queue_count, c_queue_flags, NULL)) continue;
        if(!checkDeviceExtensions(vk_physical_device, c_device_extension_count, c_device_extensions)) continue;
        if(!checkDeviceFeatures(vk_physical_device, &c_device_features)) continue;
        if(!checkDeviceMemory(vk_physical_device, c_memory_block_count, c_memory_block_descriptors)) continue;
        u32 score = rateDeviceScore(vk_physical_device, NULL);
        if(score >= best_score) {
            s_vulkan_context.physical_device = vk_physical_device;
            best_score = score;
        }
    }
    ERROR_CATCH(!s_vulkan_context.physical_device) {
        _INVOKE_CALLBACK(VK_ERR_NO_SUITABLE_GPU)
    }

    // queue layout for device create info
    layoutDeviceQueues(s_vulkan_context.physical_device, c_queue_count, c_queue_flags, s_queue_context.queue_locators);
    u32 queue_family_count = 0;
    QueueLocator* queue_families = alloca(sizeof(QueueLocator) * c_queue_count);
    combineQueuesToFamilies(c_queue_count, s_queue_context.queue_locators, &queue_family_count, queue_families);
    
    VkDeviceQueueCreateInfo* queue_infos = alloca(sizeof(VkDeviceQueueCreateInfo) * c_queue_count);
    f32* queue_priorities = alloca(sizeof(f32) * c_queue_count); //@(Mitro): this is vk bullshit
    for(u32 i = 0; i < c_queue_count; i++) {
        queue_priorities[i] = 1.0f;
    } //@(Mitro): same reason as previous line
    for(u32 i = 0; i < queue_family_count; i++) {
        queue_infos[i] = (VkDeviceQueueCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_families[i].family_id,
            .queueCount = queue_families[i].local_id,
            .pQueuePriorities = queue_priorities
        };
        queue_priorities += queue_families[i].local_id;
    }

    // device creation
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature = (VkPhysicalDeviceDynamicRenderingFeaturesKHR) {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE,
        .pNext = NULL
    };
    VkDeviceCreateInfo device_create_info = (VkDeviceCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .enabledExtensionCount = c_device_extension_count,
        .ppEnabledExtensionNames = c_device_extensions,
        .queueCreateInfoCount = queue_family_count,
        .pQueueCreateInfos = queue_infos,
        .pEnabledFeatures = &c_device_features,
        .pNext = &dynamic_rendering_feature
    };
    ERROR_CATCH(vkCreateDevice(s_vulkan_context.physical_device, &device_create_info, NULL, &s_vulkan_context.device) != VK_SUCCESS) {
        _INVOKE_CALLBACK(VK_ERR_DEVICE_CREATE)
    }
    for(u32 i = 0; i < c_queue_count; i++) {
        vkGetDeviceQueue(s_vulkan_context.device, s_queue_context.queue_locators[i].family_id, s_queue_context.queue_locators[i].local_id, s_queue_context.queues + i);
    }

    // device extesnions loading    
    _LOAD_EXT_FUNC(s_ext_context.cmd_begin_rendering, vkCmdBeginRenderingKHR)
    _LOAD_EXT_FUNC(s_ext_context.cmd_end_rendering, vkCmdEndRenderingKHR)

    // swapchain creation
    getScreenDescriptor(s_vulkan_context.window, s_vulkan_context.surface, s_vulkan_context.physical_device, &s_swapchain_context.descriptor);
    VkSwapchainCreateInfoKHR swapchain_info = (VkSwapchainCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = s_vulkan_context.surface,
        .minImageCount = s_swapchain_context.descriptor.min_image_count,
        .imageFormat = s_swapchain_context.descriptor.color_format.format,
        .imageColorSpace = s_swapchain_context.descriptor.color_format.colorSpace,
        .imageExtent = s_swapchain_context.descriptor.extent,
        .preTransform = s_swapchain_context.descriptor.surface_transform,
        .presentMode = s_swapchain_context.descriptor.present_mode,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .clipped = TRUE
    };
    ERROR_CATCH(vkCreateSwapchainKHR(s_vulkan_context.device, &swapchain_info, NULL, &s_swapchain_context.swapchain) != VK_SUCCESS) {
        _INVOKE_CALLBACK(VK_ERR_SWAPCHAIN_CREATE)
    }

    vkGetSwapchainImagesKHR(s_vulkan_context.device, s_swapchain_context.swapchain, &s_swapchain_context.image_count, NULL);
    ERROR_CATCH(s_swapchain_context.image_count > SWAPCHAIN_MAX_IMAGE_COUNT) {
        _INVOKE_CALLBACK(VK_ERR_SWAPCHAIN_TOO_MANY_IMAGES)
    }
    vkGetSwapchainImagesKHR(s_vulkan_context.device, s_swapchain_context.swapchain, &s_swapchain_context.image_count, s_swapchain_context.images);
    VkImageViewCreateInfo view_info = (VkImageViewCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = NULL, // set in for loop
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = s_swapchain_context.descriptor.color_format.format,
        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1
    };
    for(u32 i = 0; i < s_swapchain_context.image_count; i++) {
        view_info.image = s_swapchain_context.images[i];
        ERROR_CATCH(vkCreateImageView(s_vulkan_context.device, &view_info, NULL, s_swapchain_context.views + i) != VK_SUCCESS) {
            _INVOKE_CALLBACK(VK_ERR_SWAPCHAIN_VIEW_CREATE)
        }
    }

_end:
    return return_code;
}

void coreTerminate(void) {
    for(u32 i = 0; i < s_swapchain_context.image_count; i++) {
        SAFE_DESTROY(s_swapchain_context.views[i], vkDestroyImageView(s_vulkan_context.device, s_swapchain_context.views[i], NULL))
    }
    SAFE_DESTROY(s_swapchain_context.swapchain, vkDestroySwapchainKHR(s_vulkan_context.device, s_swapchain_context.swapchain, NULL))

    s_ext_context = (ExtContext){0}; // set extension ptrs to NULL

    SAFE_DESTROY(s_vulkan_context.device, vkDestroyDevice(s_vulkan_context.device, NULL))
    SAFE_DESTROY(s_vulkan_context.surface, vkDestroySurfaceKHR(s_vulkan_context.instance, s_vulkan_context.surface, NULL))
    if(s_debug_messenger) {
        ext_destroy_debug_messenger(s_vulkan_context.instance, s_debug_messenger, NULL);
        ext_create_debug_messenger = NULL;
        ext_destroy_debug_messenger = NULL;
    }
    
    ext_create_debug_messenger = NULL;
    ext_destroy_debug_messenger = NULL;

    SAFE_DESTROY(s_vulkan_context.instance, vkDestroyInstance(s_vulkan_context.instance, NULL))
    SAFE_DESTROY(s_vulkan_context.window, glfwDestroyWindow(s_vulkan_context.window))
    glfwTerminate();
    s_vulkan_context.callback = NULL;
}

// ======================================================== INTERNAL
// =================================================================

// this function should be called when window is resized
result recreateSwapchain(void) {
    result return_code = CODE_SUCCESS;
    for(u32 i = 0; i < s_swapchain_context.image_count; i++) {
        SAFE_DESTROY(s_swapchain_context.views[i], vkDestroyImageView(s_vulkan_context.device, s_swapchain_context.views[i], NULL))
    }
    SAFE_DESTROY(s_swapchain_context.swapchain, vkDestroySwapchainKHR(s_vulkan_context.device, s_swapchain_context.swapchain, NULL))

    i32 width, height;
    glfwGetFramebufferSize(s_vulkan_context.window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(s_vulkan_context.window, &width, &height);
        glfwWaitEvents();
    }
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_vulkan_context.physical_device, s_vulkan_context.surface, &capabilities);
    s_swapchain_context.descriptor.extent = (VkExtent2D){
        .width = CLAMP((u32)width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        .height = CLAMP((u32)height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
    
    VkSwapchainCreateInfoKHR swapchain_info = (VkSwapchainCreateInfoKHR) {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = s_vulkan_context.surface,
        .minImageCount = s_swapchain_context.descriptor.min_image_count,
        .imageFormat = s_swapchain_context.descriptor.color_format.format,
        .imageColorSpace = s_swapchain_context.descriptor.color_format.colorSpace,
        .imageExtent = s_swapchain_context.descriptor.extent,
        .preTransform = s_swapchain_context.descriptor.surface_transform,
        .presentMode = s_swapchain_context.descriptor.present_mode,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .clipped = TRUE
    };
    if(vkCreateSwapchainKHR(s_vulkan_context.device, &swapchain_info, NULL, &s_swapchain_context.swapchain) != VK_SUCCESS) {
        _INVOKE_CALLBACK(VK_ERR_SWAPCHAIN_CREATE)
    }

    vkGetSwapchainImagesKHR(s_vulkan_context.device, s_swapchain_context.swapchain, &s_swapchain_context.image_count, NULL); 
    // @(Mitro): might cause issues if s_swapchain_image_count too big, but it shouldn't pass first sapchain creation in vulkanInit
    vkGetSwapchainImagesKHR(s_vulkan_context.device, s_swapchain_context.swapchain, &s_swapchain_context.image_count, s_swapchain_context.images);
    VkImageViewCreateInfo view_info = (VkImageViewCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = NULL, // set in for loop
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = s_swapchain_context.descriptor.color_format.format,
        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1
    };
    for(u32 i = 0; i < s_swapchain_context.image_count; i++) {
        view_info.image = s_swapchain_context.images[i];
        if(vkCreateImageView(s_vulkan_context.device, &view_info, NULL, s_swapchain_context.views + i) != VK_SUCCESS) {
            _INVOKE_CALLBACK(VK_ERR_SWAPCHAIN_CREATE)
        }
    }
_end:
    return return_code;
}

const VulkanContext* getVulkanContextPtr(void) {
    return &s_vulkan_context;
}
const SwapchainContext* getSwapchainContextPtr(void) {
    return &s_swapchain_context;
}
const QueueContext* getQueueContextPtr(void) {
    return &s_queue_context;
}
const ExtContext* getExtensionContextPtr(void) {
    return &s_ext_context;
}
