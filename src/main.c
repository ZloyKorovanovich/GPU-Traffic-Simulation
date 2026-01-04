#include "main.h"

typedef struct {
    u64 frame_time_ms;
    u64 begin_time_sec;
    f64 time_sec;
} TimeValues;

/* time values */
static TimeValues s_time_values = (TimeValues){0};

/* message callback for logs */
b32 msgCallback(i32 msg_code, const char* msg) {
    /* if message is error we stop */
    if(MSG_IS_ERROR(msg_code)) {
        if(msg) printf("!: %d %s\n", msg_code, msg);
        else printf("ERROR: %d\n", msg_code);
        return TRUE;
    }
    /* if message is warning, we just print it and continue */
    if(MSG_IS_WARNING(msg_code)) {
        if(msg) printf("?: %d %s\n", msg_code, msg);
        else printf("?: %d\n", msg_code);
        return FALSE;
    }
    /* log message */
    if(msg) printf(":: %s\n", msg);
    return FALSE;
}

/* uniform buffer host mutable */
typedef struct {
    f32 screen_params[4];
    f32 time_params[4];
} UniformBuffer;
/* cube buffer host mutable, used only in start */
typedef struct {
    f32 vertices[36 * 4];
} CubeBuffer;
/* grid buffer host immutable, modiffied by compute shader */
typedef struct {
    f32 positions[16 * 4];
} GridBuffer;

/* callback on start, used for transfer */
void startCallback(VulkanCmdContext* cmd) {
    CubeBuffer* cube_buffer = cmdBeginWriteResource(cmd, &(RenderBinding){1, 0});
    *cube_buffer = (CubeBuffer) {
        .vertices =  {
            -1.0, -1.0,  1.0, 1.0,
            1.0, -1.0,  1.0, 1.0,
            1.0,  1.0,  1.0, 1.0,
            
            -1.0, -1.0,  1.0, 1.0,
            1.0,  1.0,  1.0, 1.0,
            -1.0,  1.0,  1.0, 1.0,

            1.0, -1.0, -1.0, 1.0,
            -1.0, -1.0, -1.0, 1.0,
            -1.0,  1.0, -1.0, 1.0,
            
            1.0, -1.0, -1.0, 1.0,
            -1.0,  1.0, -1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,

            -1.0,  1.0,  1.0, 1.0,
            1.0,  1.0,  1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,
            
            -1.0,  1.0,  1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,
            -1.0,  1.0, -1.0, 1.0,
            
            -1.0, -1.0, -1.0, 1.0,
            1.0, -1.0, -1.0, 1.0,
            1.0, -1.0,  1.0, 1.0,
            
            -1.0, -1.0, -1.0, 1.0,
            1.0, -1.0,  1.0, 1.0,
            -1.0, -1.0,  1.0, 1.0,

            1.0, -1.0,  1.0, 1.0,
            1.0, -1.0, -1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,
            
            1.0, -1.0,  1.0, 1.0,
            1.0,  1.0, -1.0, 1.0,
            1.0,  1.0,  1.0, 1.0,

            -1.0, -1.0, -1.0, 1.0,
            -1.0, -1.0,  1.0, 1.0,
            -1.0,  1.0,  1.0, 1.0,
            
            -1.0, -1.0, -1.0, 1.0,
            -1.0,  1.0,  1.0, 1.0,
            -1.0,  1.0, -1.0, 1.0
        }
    };
    cmdEndWriteResource(cmd);
    /* reset timer */
    s_time_values.begin_time_sec = getTimeSec();
    s_time_values.frame_time_ms = getTimeMs();
}

/* update callback*/
void updateCallback(const RenderWindowContext* window_context, VulkanCmdContext* cmd) {
    u64 current_time_ms = getTimeMs();
    f64 delta_time = (f64)(current_time_ms - s_time_values.frame_time_ms) / 1000.0;

    s_time_values.frame_time_ms = current_time_ms;
    s_time_values.time_sec += delta_time;

    UniformBuffer* uniform_buffer = cmdBeginWriteResource(cmd, &(RenderBinding){0, 0});
    *uniform_buffer = (UniformBuffer) {
        .screen_params = {
            (f32)window_context->x,
            (f32)window_context->y,
            1.0, -1.0
        },
        .time_params = {
            (f32)s_time_values.time_sec,
            (f32)delta_time,
            0.0, 0.0
        }
    };
    cmdEndWriteResource(cmd);

    cmdCompute(cmd, 0, 1, 1, 1);
    cmdDraw(cmd, 1, 36, 16);
}

i32 main(i32 argc, char** argv) {
    VulkanContext* vulkan_context = NULL;
    RenderContext* render_context = NULL;
    /* fill everything */
    const VulkanContextInfo vulkan_info = {
        .name = "Wreck Demo",
        .x = 800,
        .y = 600,
        .flags = VULKAN_FLAG_WIN_RESIZE,
        .version = MAKE_VERSION(0, 1, 0)
    };

    RenderResourceInfo resource_infos[] = {
        (RenderResourceInfo) {
            .binding = 0, .set = 0,
            .mutability = RENDER_RESOURCE_HOST_MUTABLE_ALWAYS,
            .type = RENDER_RESOURCE_TYPE_UNIFORM_BUFFER,
            .size = sizeof(UniformBuffer)
        },
        (RenderResourceInfo) {
            .binding = 1, .set = 0,
            .mutability = RENDER_RESOURCE_HOST_MUTABLE_START,
            .type = RENDER_RESOURCE_TYPE_STORAGE_BUFFER,
            .size = sizeof(CubeBuffer)
        },
        (RenderResourceInfo) {
            .binding = 2, .set = 0,
            .mutability = RENDER_RESOURCE_HOST_IMMUTABLE,
            .type = RENDER_RESOURCE_TYPE_STORAGE_BUFFER,
            .size = sizeof(GridBuffer)
        },
        (RenderResourceInfo) {
            .binding = 3, .set = 0,
            .mutability = RENDER_RESOURCE_HOST_IMMUTABLE,
            .type = RENDER_RESOURCE_TYPE_STORAGE_BUFFER,
            .size = sizeof(GridBuffer)
        }
    };

    RenderPipelineInfo pipeline_infos[] = {
        [0] = (RenderPipelineInfo) {
            .type = RENDER_PIPELINE_TYPE_COMPUTE,
            .name = "grid",
            .compute_shader = "out/data/grid_c.spv",
            .resources_access = (RenderResourceAccess[]) {
                {0, 0, RENDER_RESOURCE_ACCESS_TYPE_READ},
                {2, 0, RENDER_RESOURCE_ACCESS_TYPE_WRITE}
            },
            .resource_access_count = 2
        },
        [1] = (RenderPipelineInfo) {
            .type = RENDER_PIPELINE_TYPE_GRAPHICS,
            .name = "cube",
            .vertex_shader = "out/data/cube_v.spv",
            .fragment_shader = "out/data/cube_f.spv",
            .resources_access = (RenderResourceAccess[]) {
                {0, 0, RENDER_RESOURCE_ACCESS_TYPE_READ}
            },
            .resource_access_count = 1
        }
    };

    const RenderContextInfo render_info = (RenderContextInfo) {
        .resource_count = ARRAY_COUNT(resource_infos),
        .resource_infos = resource_infos,
        .pipeline_count = ARRAY_COUNT(pipeline_infos),
        .pipeline_infos = pipeline_infos,
        .update_callback = &updateCallback,
        .start_callback = &startCallback
    };
    /* run vulkan rendering! */
    if(MSG_IS_ERROR(createVulkanContext(&vulkan_info, &msgCallback, &vulkan_context))) return -1;
    if(MSG_IS_ERROR(createRenderContext(vulkan_context, &render_info, &msgCallback, &render_context))) return -2;
    if(MSG_IS_ERROR(renderLoop(vulkan_context, &msgCallback, render_context))) return -3;
    if(MSG_IS_ERROR(destroyRenderContext(vulkan_context, msgCallback, render_context))) return -3;
    if(MSG_IS_ERROR(destroyVulkanContext(&msgCallback, vulkan_context))) return -4;
}
