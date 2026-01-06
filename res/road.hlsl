struct RoadSegment {
    float2 begin;
    float2 end;
    float begin_shift;
    float end_shift;
    float width;
    float length;
};

[[vk::binding(0, 0)]] cbuffer UniformBuffer {
    float4 screen_params;
    float4 time_params;
}
[[vk::binding(2, 0)]] StructuredBuffer<RoadSegment> road_segments;


static float2 vertices[6] = {
    float2(-1.0,-1.0),
    float2( 1.0,-1.0),
    float2( 1.0, 1.0),

    float2(-1.0,-1.0),
    float2( 1.0, 1.0),
    float2(-1.0, 1.0)
};

float2 rotate(float2 p, float2 d) {
    return float2(p.x * d.x - p.y * d.y, p.x * d.y + p.y * d.x);
}

float4 vertexMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) : SV_Position {
    RoadSegment segment = road_segments[instance_id];
    float2 segment_dir = segment.end - segment.begin;
    float2 segment_center = segment_dir * 0.5 + segment.begin;
    segment_dir = segment_dir / segment.length;
    
    float2 vertex_pos = vertices[vertex_id];
    vertex_pos.x *= (segment.length * 0.5 + vertex_pos.y * ((vertex_pos.x < 0) ? segment.begin_shift : -segment.end_shift));
    vertex_pos.y *= segment.width;

    return float4((rotate(vertex_pos, segment_dir) + segment_center) * screen_params.xy * screen_params.zw, 1.0, 1.0);
}

float4 fragmentMain(float4 position_cs : SV_Position) : SV_Target {
    return float4(0.5, 0.0, 1.0, 1.0);
}
