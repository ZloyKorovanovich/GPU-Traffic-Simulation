#include "main.h"
#define INCLUDE_VULKAN_EXTERNAL
#include "vulkan/vulkan.h"
#ifdef _WIN32
#include <windows.h>
#else 
#include <pthread.h>
#endif

const char* c_command_names[] = COMMAND_NAMES;


b32 debugCallback(u32 code) {
    printf("error: module %u, code %u\n", CODE_UNPACK_MODULE(code), CODE_UNPACK_CODE(code));
    return TRUE;
}


b32 upFolder(char* path) {
    u32 len = strlen(path);
    for(u32 i = len - 1; i > 0; i--) {
        if(path[i] == '\\' || path[i] == '/') {
            path[i] = '\0';
            return TRUE;
        }
    }
    return FALSE;
}

i32 main(i32 argc, char** argv) {
    char data_path[256];
    strcpy(data_path, argv[0]);
    upFolder(data_path);
    upFolder(data_path);
    strcat(data_path, "/data/");
    printf("data path: %s\n", data_path);

    ThreadCommandBuffer command_buffer = (ThreadCommandBuffer){
        .thread_lock = 0,
        .command = 0,
        .data = NULL
    };

    result vulkan_result;
    VulkanThreadBuffer vk_thread_buffer = (VulkanThreadBuffer) {
        .callback = &debugCallback,
        .data_path = data_path,
        .flags = VULKAN_FLAG_RESIZABLE,
        .width = 800,
        .height = 600,
        .command_buffer = &command_buffer,
        .return_code = &vulkan_result
    };

#ifdef _WIN32
    HANDLE vk_thread;
    vk_thread = CreateThread(NULL, 0, vulkanRun, (void*)&vk_thread_buffer, 0, 0);
    ERROR_CATCH(!vk_thread) {
        printf("failed to start vulkan thread!\n");
        return -1;
    }
#else
    pthread_t vk_thread;
    ERROR_CATCH(pthread_create(&vk_thread, NULL, vulkanRun, (void*)&vk_thread_buffer) != 0) {
        printf("failed to start vulkan thread\n");
        return -1;
    }
#endif

    char input_command[128];
    u32 command_code;
    LOOP {
        scanf("%s", input_command);
        for(u32 i = 0; i < COMMAND_COUNT; i++) {
            if(strcmp(c_command_names[i], input_command) == 0) {
                command_code = i;
                goto _parse_success;
            }
        }
        printf("wrong command!\n");

_parse_success:
        atomic_store(&command_buffer.thread_lock, 1);
        command_buffer.thread_lock = 1;
        command_buffer.command = command_code;
        command_buffer.data = NULL;
        atomic_store(&command_buffer.thread_lock, 0);
        
        if(command_code == COMMAND_CODE_EXIT) {
            break;
        }
    }

#ifdef _WIN32
    DWORD vk_join_result = WaitForSingleObject(vk_thread, I32_MAX);
    if(vk_join_result == WAIT_TIMEOUT || vk_join_result == WAIT_FAILED) {
        return -2;
    }
#else
    i32 vk_join_result = pthread_join(vk_thread, NULL); 
    if(!(vk_join_result == 0 || vk_join_result == ESRCH)) {
        return -1;
    }
#endif
    return (i32)vulkan_result;
}
