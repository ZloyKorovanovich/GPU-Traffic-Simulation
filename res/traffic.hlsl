
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
[[vk::binding(2, 0)]] StructuredBuffer<RoadSegment> road_segments;
[[vk::binding(3, 0)]] StructuredBuffer<uint2> road_indices;

#define MAX_TREE_DEPTH 4
#define EPSILON 0.0001
#define PADDING_RANGE 0.1
#define INDICES_X 128
#define INDICES_Y 128

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

    /*
            FIELD PREVIEW

        ^ dir
        |
        | 
        +---> tan       <-|<-
                     ^^^<-|<-
                  |->|||<-|<-
                <-|->^^^<-|<-
             ^^^<-|->|||<-|<-
           ->|||<-|->^^^<-|<-
        ->|->vvv<-|->|||<-|<-
        ->|->|||<-|->^^^<-|<-
        ->|->vvv<-|->|||<-|<-
        ->|->|||<-|->^^^<-|<-
        ->|->vvv<-|->|||<-|<-
        ->|->|||<-|->^^^<-|<-
        ->|->vvv<-|->|||<-|<-  */

    float tan_sign = (pos_local.x > 0) ? 1.0 : -1.0;
    float border_mask = smoothstep(segment.width - PADDING_RANGE, segment.width, abs(pos_local.x));
    float middle_mask_inv = smoothstep(0, PADDING_RANGE, abs(pos_local.x));
    float len_range_mask = (step(y_begin, pos_local.y) * step(pos_local.y, y_end));
    float2 vec = len_range_mask * tan_sign * lerp(lerp(segment_tan, segment_dir, middle_mask_inv), -segment_tan, border_mask);
    
    float vec_len = length(vec);
    return (vec_len > EPSILON) ? vec / vec_len : float2(0.0, 0.0);
}

float2 getSegmentDirection2(RoadSegment segment, float2 p) {
    float2 segment_dir = (segment.end - segment.begin) / segment.length;
    float2 segment_tan = float2(segment_dir.y, -segment_dir.x);

    float2 pos_translated = p - segment.begin;
    float2 pos_local = float2(dot(pos_translated, segment_tan), dot(pos_translated, segment_dir));

    float width_div = 1.0 / segment.width;
    float y_begin = pos_local.x * segment.begin_shift * width_div;
    float y_end   = pos_local.x * segment.end_shift * width_div + segment.length;

    float border_antifield_mask = smoothstep(segment.width - PADDING_RANGE, segment.width, abs(pos_local.x));
    float range_mask = (step(y_begin, pos_local.y) * step(pos_local.y, y_end)) * step(abs(pos_local.x), segment.width);
    float tan_sign = (pos_local.x > 0) ? 1.0 : -1.0;

    float2 vec = lerp(segment_dir, -tan_sign * segment_tan,border_antifield_mask) * range_mask;
    float vec_len = length(vec);
    return (vec_len > EPSILON) ? vec / vec_len : float2(0.0, 0.0) / 1.0;
}

uint4 sampleRoadIndices(float2 p) {
    uint2 raw = road_indices[(uint)p * INDICES_X + (uint)p * INDICES_Y];
    return uint4(
        (raw[0] & 0xffff), 
        ((raw[0] >> 16) & 0xffff), 
        (raw[1] & 0xffff),
        ((raw[1] >> 16) & 0xffff)
    );
}

[numthreads(8, 1, 1)]
void computeMain(uint3 thread_id : SV_DispatchThreadID) {
    CarTransform car_transform = car_transforms[thread_id.x];

    uint4 segment_ids = sampleRoadIndices(car_transform.position);
    float2 field_vector = 0;
    [unroll(4)]
    for(uint i = 0; i < 4; i++) {
        if(segment_ids[i] != 0xffff) {
            field_vector += getSegmentDirection2(road_segments[segment_ids[i]], car_transform.position);
        }
    }
    /* normalize forward direction */
    float forward_len = length(field_vector);
    car_transform.forward = (forward_len < EPSILON) ? 0 : field_vector / forward_len;
    
    car_transform.position += time_params.y * car_transform.forward * 0.5;
    car_transforms[thread_id.x] = car_transform;
}
