struct RoadSegment {
    float2 begin;
    float2 end;
    float begin_shift;
    float end_shift;
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
    segment_dir = segment_dir / segment.length;
    /* default tan = (0, -1) use formula x1 = x*cos(a)-y*sin(a); y1 = x*sin(a)+y*cos(a)*/
    float2 segment_tan = float2(segment_dir.y, -segment_dir.x);
    /* project pos */
    float2 pos_translated = p - segment.begin;
    float2 pos_local = float2(dot(pos_translated, segment_tan), dot(pos_translated, segment_dir));

    float y_begin = pos_local.x * segment.begin_shift / segment.width;
    float y_end   = pos_local.x * segment.end_shift / segment.width + segment.length;

    return sign(pos_local.x) * step(abs(pos_local.x), segment.width) * (step(y_begin, pos_local.y) * step(pos_local.y, y_end)) * segment_dir;
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
