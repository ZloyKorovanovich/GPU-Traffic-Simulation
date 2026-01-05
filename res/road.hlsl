struct RoadSegment {
    float2 begin;
    float2 end;
    float width;
    float length;
};

[[vk::binding(0, 0)]] cbuffer UniformBuffer {
    float4 screen_params;
    float4 time_params;
}
[[vk::binding(2, 0)]] StructuredBuffer<RoadSegment> road_segments;


static float2 vertices[6] = {
    // First triangle (bottom-right)
    float2(-0.5, -0.5),  // 0: Bottom-left
    float2( 0.5, -0.5),  // 1: Bottom-right
    float2( 0.5,  0.5),  // 2: Top-right
    
    // Second triangle (top-left)
    float2(-0.5, -0.5),  // 3: Bottom-left (duplicate)
    float2( 0.5,  0.5),  // 4: Top-right (duplicate)
    float2(-0.5,  0.5)   // 5: Top-left
};

float2 rotate(float2 p, float2 d) {
    return float2(p.x * d.x - p.y * d.y, p.x * d.y + p.y * d.x);
}

float4 vertexMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) : SV_Position {
    RoadSegment segment = road_segments[instance_id];
    float2 segment_dir = segment.end - segment.begin;
    float2 segment_center = segment_dir * 0.5 + segment.begin;
    float  segment_len = length(segment_dir);
    segment_dir = segment_dir / segment_len;
    
    float2 vertex_pos = vertices[vertex_id];
    vertex_pos.x *= segment_len;
    vertex_pos.y *= segment.width * 2;

    return float4((rotate(vertex_pos, segment_dir) + segment_center) * screen_params.zw* 300 / screen_params.xy, 1.0, 1.0);
}

float4 fragmentMain(float4 position_cs : SV_Position) : SV_Target {
    return float4(0.5, 0.0, 1.0, 1.0);
}
