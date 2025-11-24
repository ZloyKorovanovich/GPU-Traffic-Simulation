#define INCLUDE_VULKAN_EXTERNAL
#define INCLUDE_VULKAN_INTERNAL
#include "vulkan.h"

static const ThreadCommandBuffer* s_thread_command_buffer;

#define ERROR_CATCH_CALL(expr) call_result = expr; if(call_result != CODE_SUCCESS) {goto _end;}

result updateCallback(f64 time, f64 delta) {
    result return_code = CODE_SUCCESS;
    atomic_uint thread_lock = atomic_load(&s_thread_command_buffer->thread_lock);
    if(thread_lock != 0) {
        goto _end;
    }

    switch (s_thread_command_buffer->command) {
        case COMMAND_CODE_NONE:
            goto _end;
        case COMMAND_CODE_EXIT:
            renderLoopExit();
            goto _end;
    }
_end:
    return return_code;
}

result vulkanMain(void* arg) {
    VulkanThreadBuffer thread_buffer = *((VulkanThreadBuffer*)arg);
    result call_result = CODE_SUCCESS;
    s_thread_command_buffer = thread_buffer.command_buffer;
    ERROR_CATCH_CALL(coreInit(thread_buffer.width, thread_buffer.height, thread_buffer.flags, thread_buffer.callback))
    ERROR_CATCH_CALL(vramInit(NULL))
    ERROR_CATCH_CALL(resourcesInit(thread_buffer.data_path))
    ERROR_CATCH_CALL(renderInit(NULL))
    ERROR_CATCH_CALL(renderLoop(&updateCallback))
_end:
    renderTerminate();
    s_thread_command_buffer = NULL;
    resourcesTerminate();
    vramTerminate();
    coreTerminate();
    *thread_buffer.return_code = call_result;

    return call_result;
}

#ifdef _WIN32
DWORD WINAPI vulkanRun(void* arg) {
    vulkanMain(arg);
    return 0;
}
#else
void* vulkanRun(void* arg) {
    vulkanMain(arg);
    return NULL;
}
#endif
