struct CarTransform {
    float2 position;
    float2 forward;
};

[[vk::binding(0, 0)]] cbuffer UniformBuffer {
    float4 screen_params;
    float4 time_params;
}
[[vk::binding(1, 0)]] StructuredBuffer<CarTransform> car_transforms;


static float2 vertices[3] = {
    float2(-0.5, 0.5),   // Right-pointing tip (top-left)
    float2(-0.5, -0.5),  // Back corner (bottom-left)
    float2(0.5, 0.0)     // Tip pointing right
};

float2 rotate(float2 p, float2 d) {
    return float2(p.x * d.x - p.y * d.y, p.x * d.y + p.y * d.x);
}

float4 vertexMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) : SV_Position {
    CarTransform car_transform = car_transforms[instance_id];
    car_transform.forward = dot(car_transform.forward, car_transform.forward) > 0.0001 ? car_transform.forward : float2(0, 1);
    return float4((rotate(vertices[vertex_id], car_transform.forward) * 0.1 + car_transform.position) * screen_params.zw * 300.0 / screen_params.xy, 0.5, 1.0);
}

float4 fragmentMain(float4 position_cs : SV_Position) : SV_Target {
    return float4(1, 1, 1, 1);
}
