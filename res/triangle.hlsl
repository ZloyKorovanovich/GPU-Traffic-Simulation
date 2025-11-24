static float2 positions[3] = {
    float2( 0.5, 0.5),
    float2(-0.5, 0.5),
    float2( 0.0,-0.2)
};

static float3 colors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

struct Interpolators {
    float4 position_cs : SV_POSITION;
    float4 color : COLOR;
};

[[vk::binding(0, 0)]] cbuffer global_ubuffer {
    uniform float4 _screen_size;
    uniform float _time;
    uniform float _delta;
};
[[vk::binding(1, 0)]] StructuredBuffer<float4> position_buffer;

Interpolators vertexFunc(uint vertex_id : SV_VERTEXID) {
    Interpolators output = (Interpolators)0;
    uint sample_id = vertex_id / 3;
    uint local_id = vertex_id - sample_id * 3;
    output.position_cs = float4(float2(position_buffer[sample_id].xy + positions[local_id].xy) * 512 / _screen_size.zw, position_buffer[sample_id].z, 1);
    output.color = float4(colors[local_id], 1);
    return output;
}

struct Pixel {
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

Pixel fragmentFunc(Interpolators input){
    Pixel output = (Pixel)0;
    output.color = input.color;
    output.depth = input.position_cs.z;
    return output;
}
