/*  To find closest node you need to iterate through tree.
    Tree node array is structured so, that layers are grouped together, for example:

            Node A
            /   \
           /     \
       Node B   Node F
       /   \       \
      /     \       \
  Node C  Node D    ... 

    This will look like that inside of array:
    
    [Node A][Node B][Node F][Node C][Node D][...]

    Number of barnches is different from example, as default we use 4 branches per node.
    Each branch represents index of the next node. If id is 0, its invalid, and branch doesnt exist.
    Recursion from node to the same node through index of itself is prohibited (thats why nothing can point to id 0 which is first node).
    if index is greated than 0xffff, index is pointing to the road array, to the actual road segment.
    if index is equal to 0xfffffffff, its emoty branch, pointing to "NULL" road segment. 
*/

struct SpaceNode {
    uint4 branches;
    /* +---+
       |x y| 
       |z w| 
       +---+ */
};

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
[[vk::binding(3, 0)]] StructuredBuffer<SpaceNode> road_tree;

#define MAX_TREE_DEPTH 4
#define EPSILON 0.0001
#define PADDING_RANGE 0.1

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

/* implement this tree search */
void getSegments(float2 position, out RoadSegment segments[4]) {
    uint node_id = 0;
    SpaceNode node = road_tree[0];
    uint2 position_id = uint2(position * MAX_TREE_DEPTH);
    uint4 road_ids = uint4(0xffffffff);
    /* search though tree */
    uint i;
    [unroll(MAX_TREE_DEPTH)] for(i = 1; i < MAX_TREE_DEPTH; i++) {
        uint2 sample_id = position_id / MAX_TREE_DEPTH - i;
        uint next_node_id = node.branches[sample_id.x + sample_id.y * 2];
        /* I hope compiler is smart enough to recognize that operations are the same */
        if(next_node_id == 0xffffffff) {
            node_id = node_id;
            node = road_tree[node_id];
            road_ids = uint4(0xffffffff);
        } else {
            node_id = next_node_id;
            node = road_tree[node_id];
            road_ids = node.branches;
        }
    }
    /* by that moment indices of roads are accumulated in road_ids */
    [unroll(4)] for(i = 0; i < 4; i++) {
        segments[i] = road_segments[(road_ids[i] == 0xffffffff) ? 0 : road_segments[road_ids[i]]];
    }
}

[numthreads(8, 1, 1)]
void computeMain(uint3 thread_id : SV_DispatchThreadID) {
    CarTransform car_transform = car_transforms[thread_id.x];
    RoadSegment segments[4];
    getSegments(car_transform.position, segments);

    car_transform.forward = getSegmentDirection2(segments[0], car_transform.position);
    car_transform.forward += getSegmentDirection2(segments[1], car_transform.position);
    car_transform.forward += getSegmentDirection2(segments[2], car_transform.position);
    car_transform.forward += getSegmentDirection2(segments[3], car_transform.position);

    float forward_len_sqr = dot(car_transform.forward, car_transform.forward);

    car_transform.forward = car_transform.forward / sqrt(forward_len_sqr); //(forward_len_sqr > ESPSILON) ? car_transform.forward / sqrt(forward_len_sqr) : float2(0, 0);
    
    car_transform.position += time_params.y * car_transform.forward * 0.5;
    car_transforms[thread_id.x] = car_transform;
}
