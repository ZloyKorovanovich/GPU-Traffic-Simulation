
struct Car {
    float2 position;
    float2 direction;
};
struct RoadSegment {
    float2 position;
    float2 rotation;
    float2 width;
    float2 length;
};


[[vk::binding(0, 0)]] cbuffer global_ubuffer {
    uniform float4 _screen_size;
    uniform float _time;
    uniform float _delta;
};

[[vk::binding(1, 0)]] RWStructuredBuffer<Car> rw_car_buffer;
[[vk::binding(2, 0)]] StructuredBuffer<RoadSegment> r_segment_buffer;

#define INVALID_ID 0xffffffff


float2 getFieldVector(float2 position, RoadSegment segment) {
    float2 direction = float2(segment.rotation.x - segment.rotation.y, 0);
    float2 tangent = float2(0, segment.rotation.y - segment.rotation.x);
    float2 local_pos = position - segment.position;
    float w_mask = dot(local_pos, direction);
    float l_mask = dot(local_pos, tangent);

    float left_mask = smoothstep(segment.width.x, segment.width.x + segment.width.y, w_mask);
    float right_mask = smoothstep(segment.width.x, segment.width.x + segment.width.y, -w_mask);

    float2 motion_vec = (
        (1 - left_mask) * smoothstep(0.0, segment.width.y, w_mask) * -direction +
        (1 - right_mask) * smoothstep(0.0, segment.width.y, -w_mask) * direction
    ) * (1 - smoothstep(segment.length.x, segment.length.x + segment.length.y, abs(l_mask))) + 
    left_mask * tangent + right_mask * -tangent;
    
    return motion_vec;
}

[numthreads(64, 1, 1)]
void computeFunc(uint3 global_id : SV_DISPATCHTHREADID) {
    Car car = rw_car_buffer[global_id.x];

    RoadSegment road = r_segment_buffer[0];
    car.position += getFieldVector(car.position, road) * _time;

    rw_car_buffer[global_id.x] = car;
}
