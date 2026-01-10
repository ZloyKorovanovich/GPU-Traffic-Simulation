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

typedef struct {
    f32 position[2];
    f32 forward[2];
} CarTransform;

typedef struct {
    f32 begin[2];
    f32 end[2];
    float begin_shift;
    float end_shift;
    float width;
    float length;
} RoadSegment;

typedef struct {
    u32 branches[4];
} SpaceNode;

typedef enum {
    PIPELINE_TRAFFIC_ID = 0,
    PIPELINE_CAR_ID = 1,
    PIPELINE_ROAD_ID = 2
} PIPLEINE_IDS;

/* uniform buffer host mutable */
typedef struct {
    f32 screen_params[4];
    f32 time_params[4];
} UniformBuffer;
/* cube buffer host mutable, used only in start */
typedef struct {
    CarTransform car_transforms[8];
} CarBuffer;
typedef struct {
    RoadSegment road_segments[4];
} RoadBuffer;
typedef struct {
    SpaceNode road_nodes[];
} RoadTree;

/* resources */
const RenderResourceInfo c_resource_infos[] = {
    (RenderResourceInfo) {
        .binding = 0, .set = 0,
        .mutability = RENDER_RESOURCE_HOST_MUTABLE_ALWAYS,
        .type = RENDER_RESOURCE_TYPE_UNIFORM_BUFFER,
        .size = sizeof(UniformBuffer)
    },
    (RenderResourceInfo) {
        .binding = 1, .set = 0,
        .mutability = RENDER_RESOURCE_HOST_MUTABLE_ALWAYS,
        .type = RENDER_RESOURCE_TYPE_STORAGE_BUFFER,
        .size = sizeof(CarBuffer)
    },
    (RenderResourceInfo) {
        .binding = 2, .set = 0,
        .mutability = RENDER_RESOURCE_HOST_MUTABLE_START,
        .type = RENDER_RESOURCE_TYPE_STORAGE_BUFFER,
        .size = sizeof(RoadBuffer)
    }
};

/* pipelines */
const RenderPipelineInfo c_pipeline_infos[] = {
    [PIPELINE_TRAFFIC_ID] = (RenderPipelineInfo) {
        .type = RENDER_PIPELINE_TYPE_COMPUTE,
        .name = "traffic",
        .compute_shader = "shaders/traffic_c.spv",
        .resources_access = (RenderResourceAccess[]) {
            {0, 0, RENDER_RESOURCE_ACCESS_TYPE_READ},
            {1, 0, RENDER_RESOURCE_ACCESS_TYPE_WRITE},
            {2, 0, RENDER_RESOURCE_ACCESS_TYPE_READ}
        },
        .resource_access_count = 3
    },
    [PIPELINE_CAR_ID] = (RenderPipelineInfo) {
        .type = RENDER_PIPELINE_TYPE_GRAPHICS,
        .name = "car",
        .vertex_shader = "shaders/car_v.spv",
        .fragment_shader = "shaders/car_f.spv",
        .resources_access = (RenderResourceAccess[]) {
            {0, 0, RENDER_RESOURCE_ACCESS_TYPE_READ},
            {1, 0, RENDER_RESOURCE_ACCESS_TYPE_READ}
        },
        .resource_access_count = 2
    },
    [PIPELINE_ROAD_ID] = (RenderPipelineInfo) {
        .type = RENDER_PIPELINE_TYPE_GRAPHICS,
        .name = "road",
        .vertex_shader = "shaders/road_v.spv",
        .fragment_shader = "shaders/road_f.spv",
        .resources_access = (RenderResourceAccess[]) {
            {0, 0, RENDER_RESOURCE_ACCESS_TYPE_READ},
            {2, 0, RENDER_RESOURCE_ACCESS_TYPE_READ}
        },
        .resource_access_count = 2
    }
};

/* callback on start, used for transfer */
void startCallback(VulkanCmdContext* cmd) {
    *(CarBuffer*)cmdBeginWriteResource(cmd, &(RenderBinding){1, 0}) = (CarBuffer) {
        .car_transforms = {
            {{-0.9,-0.9}, {0.0, 1.0}},
            {{-0.6,-0.8}, {0.0, 1.0}},
            {{ 0.8, 0.6}, {0.0, 1.0}},
            {{-0.5,-0.9}, {0.0, 1.0}},
            {{ 0.1, 0.8}, {0.0, 1.0}},
            {{ 0.8,-0.6}, {0.0, 1.0}},
            {{ 0.6, 0.8}, {0.0, 1.0}},
            {{ 0.8, 0.2}, {0.0, 1.0}}
        }
    };
    cmdEndWriteResource(cmd);
    *(RoadBuffer*)cmdBeginWriteResource(cmd, &(RenderBinding){2, 0}) = (RoadBuffer) {
        .road_segments = {
            {{-0.85,-0.85}, {-0.85, 0.85}, 0.1,-0.1, 0.1, 1.7},
            {{-0.85, 0.85}, { 0.85, 0.85}, 0.1,-0.1, 0.1, 1.7},
            {{ 0.85, 0.85}, { 0.85,-0.85}, 0.1,-0.1, 0.1, 1.7},
            {{ 0.85,-0.85}, {-0.85,-0.85}, 0.1,-0.1, 0.1, 1.7}
        }
    };
    cmdEndWriteResource(cmd);

    /* reset timer */
    s_time_values.begin_time_sec = getTimeSec();
    s_time_values.frame_time_ms = getTimeMs();
}

/* update callback called every frame */
void updateCallback(const RenderWindowContext* window_context, VulkanCmdContext* cmd) {
    u64 current_time_ms = getTimeMs();
    /* if delta real time difference is too big, we should prevent gpu time-dependent system breaks */
    f64 delta_time = MIN((f64)(current_time_ms - s_time_values.frame_time_ms) / 1000.0, 0.02f);

    s_time_values.frame_time_ms = current_time_ms;
    s_time_values.time_sec += delta_time;

    UniformBuffer* uniform_buffer = cmdBeginWriteResource(cmd, &(RenderBinding){0, 0});
    *uniform_buffer = (UniformBuffer) {
        .screen_params = {
            300.0 / (f32)window_context->x,
            300.0 / (f32)window_context->y,
            1.0, -1.0
        },
        .time_params = {
            (f32)s_time_values.time_sec,
            (f32)delta_time,
            0.0, 0.0
        }
    };
    cmdEndWriteResource(cmd);

    cmdCompute(cmd, PIPELINE_TRAFFIC_ID, 1, 1, 1);
    cmdDraw(cmd, PIPELINE_CAR_ID, 3, 8);
    cmdDraw(cmd, PIPELINE_ROAD_ID, 6, 4);
}

i32 main(i32 argc, char** argv) {
    VulkanContext* vulkan_context = NULL;
    RenderContext* render_context = NULL;

    char work_path[256] = {0};
    strcpy(work_path, argv[0]);
    upFolder(work_path);
    upFolder(work_path);
    strcat(work_path, "/");

    const VulkanContextInfo vulkan_info = {
        .name = "Wreck Demo",
        .directory = work_path,
        .x = 800,
        .y = 600,
        .flags = VULKAN_FLAG_WIN_RESIZE,
        .version = MAKE_VERSION(0, 1, 0)
    };
    const RenderContextInfo render_info = (RenderContextInfo) {
        .resource_count = ARRAY_COUNT(c_resource_infos),
        .resource_infos = c_resource_infos,
        .pipeline_count = ARRAY_COUNT(c_pipeline_infos),
        .pipeline_infos = c_pipeline_infos,
        .update_callback = &updateCallback,
        .start_callback = &startCallback
    };
    /* run vulkan rendering! */
    if(MSG_IS_ERROR(createVulkanContext(&vulkan_info, &msgCallback, &vulkan_context))) return -1;
    if(MSG_IS_ERROR(createRenderContext(vulkan_context, &render_info, &msgCallback, &render_context))) return -2;
    if(MSG_IS_ERROR(renderRun(vulkan_context, &msgCallback, render_context))) return -3;
    if(MSG_IS_ERROR(destroyRenderContext(vulkan_context, msgCallback, render_context))) return -3;
    if(MSG_IS_ERROR(destroyVulkanContext(&msgCallback, vulkan_context))) return -4;
}
