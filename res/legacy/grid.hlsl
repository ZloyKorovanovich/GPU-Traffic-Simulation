#include "noise.hlsl"

[[vk::binding(0, 0)]] cbuffer uniform_buffer {
    float4 screen_params;
    float4 time_params;
}
[[vk::binding(1, 0)]] StructuredBuffer<float4> vertices;

[[vk::binding(2, 0)]] RWStructuredBuffer<float4> positions;
[[vk::binding(3, 0)]] RWStructuredBuffer<float4> colors;

[numthreads(16, 1, 1)]
void computeMain(uint3 thread_id : SV_DISPATCHTHREADID) {
    float x = (float)(thread_id.x % 4) * 2.0 / 3.0 - 1.0;
    float y = (float)(thread_id.x / 4) * 2.0 / 3.0 - 1.0;
    positions[thread_id.x] = float4(x + sin((time_params.x + y) * 4) * 0.03, y + sin((time_params.x + x) * 4) * 0.03, 0.5, 1.0);
    colors[thread_id.x] = float4(noise((x + time_params.x) * 3), noise(y - time_params.x * 2), noise((time_params.x) * 4), 1.0);
}
