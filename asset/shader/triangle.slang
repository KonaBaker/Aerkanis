struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

struct TrianglePushConstants
{
    float4 viewProjectionRow0;
    float4 viewProjectionRow1;
    float4 viewProjectionRow2;
    float4 viewProjectionRow3;
    float4 triangle;
};

[[vk::push_constant]]
ConstantBuffer<TrianglePushConstants> PushConstants;

float4 applyViewProjection(float4 position)
{
    return float4(
        dot(PushConstants.viewProjectionRow0, position),
        dot(PushConstants.viewProjectionRow1, position),
        dot(PushConstants.viewProjectionRow2, position),
        dot(PushConstants.viewProjectionRow3, position));
}

[shader("vertex")]
VertexOutput vertMain(uint vertexId : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(0.0f, -0.58f),
        float2(0.58f, 0.58f),
        float2(-0.58f, 0.58f),
    };

    float3 colors[3] =
    {
        float3(1.0f, 0.22f, 0.18f),
        float3(0.14f, 0.88f, 0.42f),
        float3(0.22f, 0.44f, 1.0f),
    };

    float size = PushConstants.triangle.x;
    float angle = PushConstants.triangle.y;
    float sine = sin(angle);
    float cosine = cos(angle);
    float2 scaled = positions[vertexId] * size;
    float2 rotated = float2(
        scaled.x * cosine - scaled.y * sine,
        scaled.x * sine + scaled.y * cosine);

    VertexOutput output;
    output.position = applyViewProjection(float4(rotated, 0.0f, 1.0f));
    output.color = colors[vertexId];
    return output;
}

struct FragmentInput
{
    float3 color : COLOR0;
};

[shader("fragment")]
float4 fragMain(FragmentInput input) : SV_Target0
{
    return float4(input.color, 1.0f);
}
