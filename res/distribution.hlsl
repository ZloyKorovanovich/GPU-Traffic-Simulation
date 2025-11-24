
[[vk::binding(0, 0)]] cbuffer global_ubuffer {
    uniform float4 _screen_size;
    uniform float _time;
    uniform float _delta;
};
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> position_buffer;


float rand(float2 seed){
    return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

[numthreads(64, 1, 1)]
void computeFunc(uint3 global_id : SV_DISPATCHTHREADID) {
    float angle = rand(global_id.xx) * 3.14159 * 2;
    float displace = frac(_time * 0.1) > 0.5 ? 1 - frac(_time * 0.1) : frac(_time * 0.1); 
    position_buffer[global_id.x] = float4(float2(cos(angle), sin(angle)) * displace, global_id.x / 64.0, 0.0); 
}
