struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

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

    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
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
