#include "noise.hlsl"

[[vk::binding(0, 0)]] cbuffer uniform_buffer {
    float4 screen_params;
    float4 time_params;
}
[[vk::binding(1, 0)]] StructuredBuffer<float4> vertices;

[[vk::binding(2, 0)]] StructuredBuffer<float4> positions;
[[vk::binding(3, 0)]] StructuredBuffer<float4> colors;

#define PI 3.14159

struct Interpolators {
    float4 position_cs : SV_POSITION;
    float4 color : COLOR;
};

struct Targets {
    float4 color : SV_TARGET0;
    float depth : SV_DEPTH;
};

float3 rotate(float3 position, float angle) {
    return float3(
        position.x * cos(angle) - position.z * sin(angle),
        position.y,
        position.x * sin(angle) + position.z * cos(angle)
    );
}

Interpolators vertexMain(uint vertex_id : SV_VERTEXID, uint instance_id : SV_INSTANCEID) {
    float4 position = float4(rotate(0.03 * vertices[vertex_id].xyz, time_params.x * noise(instance_id)) + float3(0.2 * positions[instance_id].xy, positions[instance_id].z), 1.0);

    Interpolators output = (Interpolators)0;
    output.position_cs = float4(position.xy / (position.z / 4.0 - 0.01) * 300.0 / screen_params.xy * screen_params.zw, position.z, 1.0);
    output.color = (1 - (float)(vertex_id / 6) / 8.0) * colors[instance_id];

    return output;
}

Targets fragmentMain(Interpolators input) {
    Targets output = (Targets)0;
    output.color = input.color;
    output.depth = input.position_cs.z;
    return output;
}
