
struct RoadSegment {
    float2 begin;
    float2 end;
    float begin_shift;
    float end_shift;
    float width;
    float length;
};

struct RoadNode {
    uint4 nodes;
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
[[vk::binding(3, 0)]] StructuredBuffer<RoadNode> road_nodes;

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
    /* set road indices to invalid */
    uint4 sgements = uint4(
        0x7fffffff,
        0x7fffffff,
        0x7fffffff,
        0x7fffffff
    );

    /* set node to root node, which has id 0 */
    RoadNode node = road_nodes[0];

    /* when converting position to int it is crusial to adjust scale in floats */
    float scale_m = 2.0;
    /* this is unrolled compile time for MAX_TREE_DEPTH iterations */
    [unroll(MAX_TREE_DEPTH)]
    for(uint i = 0; i < MAX_TREE_DEPTH; i++) {
        /* if any index of children in node is < 0x80000000, 
            than all indices in node are indexing nodes */
        if(node.nodes[0] < 0x80000000) {
            uint node_id = (uint)(p.x * scale_m) + (uint)(p.y * scale_m) * 2;
            /* 0x7fffffff is invalid value for node index */
            if(node_id != 0x7fffffff) {
                node = road_nodes[node_id];
            }
        }
        /* if index is >= 0x80000000, all indices in node are indexing road segments */ 
        else {
            sgements = uint4(
                node.nodes[0] - 0x80000000,
                node.nodes[1] - 0x80000000,
                node.nodes[2] - 0x80000000,
                node.nodes[3] - 0x80000000
            );
        }
        scale_m *= 2.0;
    }

    return sgements;
}

[numthreads(8, 1, 1)]
void computeMain(uint3 thread_id : SV_DispatchThreadID) {
    CarTransform car_transform = car_transforms[thread_id.x];

    uint4 road_indices = sampleRoadIndices(car_transform.position);
    [unroll(4)]
    for(uint i = 0; i < 4; i++) {
        RoadSegment segment = (road_indices[i] == 0x7fffffff) ? (RoadSegment)0 : road_segments[road_indices[i]];
    }
}
