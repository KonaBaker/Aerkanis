struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]]
Texture2D<float4> CloudColor;
[[vk::binding(1, 0)]]
SamplerState CloudSampler;

[shader("vertex")]
VertexOutput vertMain(uint vertexId : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(3.0f, -1.0f),
        float2(-1.0f, 3.0f),
    };

    float2 uv[3] =
    {
        float2(0.0f, 0.0f),
        float2(2.0f, 0.0f),
        float2(0.0f, 2.0f),
    };

    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.uv = uv[vertexId];
    return output;
}

struct FragmentInput
{
    float2 uv : TEXCOORD0;
};

[shader("fragment")]
float4 fragMain(FragmentInput input) : SV_Target0
{
    float4 cloud = CloudColor.Sample(CloudSampler, input.uv);
    return float4(cloud.rgb, 1.0f);
}
