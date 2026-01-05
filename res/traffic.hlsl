struct RoadSegment {
    float2 begin;
    float2 end;
    float width;
    float length;
};

struct CarTransform {
    float2 position;
    float2 forward;
};

[[vk::binding(0, 0)]] cbuffer UniformBuffer {
    float4 screen_params;
    float4 time_params;
}
[[vk::binding(1, 0)]] RWStructuredBuffer<CarTransform> car_transforms;
[[vk::binding(2, 0)]] StructuredBuffer<RoadSegment> road;

#define ESPSILON 0.0001

float2 getSegmentDirection(RoadSegment segment, float2 p) {
    float2 segment_dir = segment.end - segment.begin;
    float  segment_len = length(segment_dir);
    segment_dir = segment_dir / segment_len;
    /* default tan = (0, -1) use formula x1 = x*cos(a)-y*sin(a); y1 = x*sin(a)+y*cos(a)*/
    float2 segment_tan = float2(segment_dir.y, -segment_dir.x);
    /* project pos */
    float2 pos_local = p - segment.begin;
    float  proj_tan  = dot(pos_local, segment_tan);
    float  proj_dir  = dot(pos_local, segment_dir);
    /* detect direction, return -1 road direction if left, + road direction if right, 0 if out of border */
    return sign(proj_tan) * step(abs(proj_tan), segment.width) * (step(proj_dir, segment_len) * step(0, proj_dir)) * segment_dir;
}

[numthreads(8, 1, 1)]
void computeMain(uint3 thread_id : SV_DispatchThreadID) {
    CarTransform car_transform = car_transforms[thread_id.x];
    car_transform.forward = getSegmentDirection(road[0], car_transform.position);
    car_transform.forward += getSegmentDirection(road[1], car_transform.position);
    car_transform.forward += getSegmentDirection(road[2], car_transform.position);
    car_transform.forward += getSegmentDirection(road[3], car_transform.position);

    float forward_len_sqr = dot(car_transform.forward, car_transform.forward);

    car_transform.forward = (forward_len_sqr > ESPSILON) ? car_transform.forward / sqrt(forward_len_sqr) : float2(0, 0);
    
    car_transform.position += time_params.y * car_transform.forward * 0.5;
    car_transforms[thread_id.x] = car_transform;
}
