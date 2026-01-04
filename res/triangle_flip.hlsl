[[vk::binding(0, 0)]] cbuffer uniform_buffer {
    float4 screen_params;
}

static float3 positions[3] = {
    float3( 0.5, 0.5, 0.0),
    float3(-0.5, 0.5, 0.0),
    float3( 0.0,-0.5, 0.0)
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

struct Pixel {
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

Interpolators vertexMain(uint vertex_id : SV_VERTEXID) {
    Interpolators output = (Interpolators)0;
    output.position_cs = float4(-positions[vertex_id].xy * 500.0 / screen_params.xy, 0.0, 1);
    output.color = float4(colors[vertex_id], 1);
    return output;
}

Pixel fragmentMain(Interpolators input) {
    Pixel output = (Pixel)0;
    output.color = input.color;
    output.depth = 1.0;
    return output;
}