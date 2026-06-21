struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct SunSkyParameters
{
    float4 inverseViewProjectionRow0;
    float4 inverseViewProjectionRow1;
    float4 inverseViewProjectionRow2;
    float4 inverseViewProjectionRow3;
    float4 cameraPositionExposure;
    float4 sunDirectionIntensity;
    float4 sunColorDaylight;
    float4 ambientColorSkyIntensity;
    float4 skyHorizonSunRadius;
    float4 skyZenithSunGlow;
    float4 skyGroundColor;
};

[[vk::binding(0, 0)]]
ConstantBuffer<SunSkyParameters> Params;

float saturate01(float value)
{
    return clamp(value, 0.0f, 1.0f);
}

float4 applyInverseViewProjection(float4 position)
{
    return float4(
        dot(Params.inverseViewProjectionRow0, position),
        dot(Params.inverseViewProjectionRow1, position),
        dot(Params.inverseViewProjectionRow2, position),
        dot(Params.inverseViewProjectionRow3, position));
}

float3 rayDirectionFromUv(float2 uv)
{
    float2 ndc = uv * 2.0f - 1.0f;
    float4 worldNear = applyInverseViewProjection(float4(ndc, 0.0f, 1.0f));
    float4 worldFar = applyInverseViewProjection(float4(ndc, 1.0f, 1.0f));
    worldNear.xyz /= max(worldNear.w, 0.0001f);
    worldFar.xyz /= max(worldFar.w, 0.0001f);
    return normalize(worldFar.xyz - worldNear.xyz);
}

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
    float3 direction = rayDirectionFromUv(input.uv);
    float horizon = saturate01(direction.y * 0.5f + 0.5f);
    float3 sky = lerp(
        Params.skyHorizonSunRadius.rgb,
        Params.skyZenithSunGlow.rgb,
        pow(horizon, 0.68f));
    sky *= Params.ambientColorSkyIntensity.w;

    float3 sunDirection = normalize(Params.sunDirectionIntensity.xyz);
    float sunCos = dot(normalize(direction), sunDirection);
    float sunRadius = max(Params.skyHorizonSunRadius.w, 0.0001f);
    float sunDisc = smoothstep(cos(sunRadius * 2.0f), cos(sunRadius), sunCos);
    float sunGlow = pow(saturate01(sunCos), 96.0f) * Params.skyZenithSunGlow.w;
    float horizonGlow = pow(1.0f - horizon, 2.0f) * max(sunDirection.y, 0.0f) * 0.18f;
    float3 sun = Params.sunColorDaylight.rgb * Params.sunDirectionIntensity.w;
    float3 color = sky + sun * (sunDisc + sunGlow * 0.22f + horizonGlow);
    color += Params.ambientColorSkyIntensity.rgb * 0.08f;
    color = 1.0f - exp(-color * Params.cameraPositionExposure.w);
    return float4(color, 1.0f);
}
