#include "cloud-nubis-cubed-params.slangh"

[[vk::binding(0, 0)]]
ConstantBuffer<CloudNubisCubedParameters> Params;
[[vk::binding(1, 0)]]
RWTexture2D<float4> OutputImage;
[[vk::binding(2, 0)]]
Texture2D<float4> CurrentImage;
[[vk::binding(3, 0)]]
Texture2D<float4> HistoryImage;
[[vk::binding(4, 0)]]
SamplerState LinearSampler;

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

float4 applyPreviousViewProjection(float4 position)
{
    return float4(
        dot(Params.previousViewProjectionRow0, position),
        dot(Params.previousViewProjectionRow1, position),
        dot(Params.previousViewProjectionRow2, position),
        dot(Params.previousViewProjectionRow3, position));
}

float3 rayDirectionFromPixel(uint2 pixel)
{
    float2 resolution = max(Params.screenAndSteps.xy, float2(1.0f, 1.0f));
    float2 uv = (float2(pixel) + 0.5f) / resolution;
    float2 ndc = uv * 2.0f - 1.0f;
    float4 worldNear = applyInverseViewProjection(float4(ndc, 0.0f, 1.0f));
    float4 worldFar = applyInverseViewProjection(float4(ndc, 1.0f, 1.0f));
    worldNear.xyz /= max(abs(worldNear.w), 0.0001f);
    worldFar.xyz /= max(abs(worldFar.w), 0.0001f);
    return normalize(worldFar.xyz - worldNear.xyz);
}

bool updateSlab(float origin, float direction, float minimum, float maximum, inout float entry, inout float exit)
{
    if (abs(direction) < 0.0001f)
    {
        return origin >= minimum && origin <= maximum;
    }

    float t0 = (minimum - origin) / direction;
    float t1 = (maximum - origin) / direction;
    float nearDistance = min(t0, t1);
    float farDistance = max(t0, t1);
    entry = max(entry, nearDistance);
    exit = min(exit, farDistance);
    return exit >= entry;
}

bool intersectBounds(float3 origin, float3 direction, out float entry, out float exit)
{
    float3 boundsMin = Params.boundsMinDataset.xyz;
    float3 boundsMax = Params.boundsMaxDensity.xyz;
    entry = 0.0f;
    exit = Params.detailAndMarch.z;

    if (!updateSlab(origin.x, direction.x, boundsMin.x, boundsMax.x, entry, exit))
    {
        return false;
    }
    if (!updateSlab(origin.y, direction.y, boundsMin.y, boundsMax.y, entry, exit))
    {
        return false;
    }
    if (!updateSlab(origin.z, direction.z, boundsMin.z, boundsMax.z, entry, exit))
    {
        return false;
    }

    entry = max(entry, 0.0f);
    exit = min(exit, Params.detailAndMarch.z);
    return exit > entry;
}

float luminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId.xy;
    float2 resolution = max(Params.screenAndSteps.xy, float2(1.0f, 1.0f));
    uint2 extent = uint2((uint)resolution.x, (uint)resolution.y);
    if (pixel.x >= extent.x || pixel.y >= extent.y)
    {
        return;
    }

    float2 uv = (float2(pixel) + 0.5f) / resolution;
    float4 currentColor = CurrentImage.SampleLevel(LinearSampler, uv, 0.0f);
    if (Params.featureFlags.z <= 0.5f || Params.featureFlags.w <= 0.5f || Params.splitAndGrid.w <= 0.001f)
    {
        OutputImage[pixel] = currentColor;
        return;
    }

    float3 cameraPosition = Params.cameraPositionTime.xyz;
    float3 rayDirection = rayDirectionFromPixel(pixel);
    float entryDistance = 0.0f;
    float exitDistance = 0.0f;
    if (!intersectBounds(cameraPosition, rayDirection, entryDistance, exitDistance))
    {
        OutputImage[pixel] = currentColor;
        return;
    }

    float anchorDistance = lerp(entryDistance, exitDistance, 0.42f);
    float3 anchorPosition = cameraPosition + rayDirection * anchorDistance;
    float4 previousClip = applyPreviousViewProjection(float4(anchorPosition, 1.0f));
    if (previousClip.w <= 0.0001f)
    {
        OutputImage[pixel] = currentColor;
        return;
    }

    float3 previousNdc = previousClip.xyz / previousClip.w;
    float2 previousUv = previousNdc.xy * 0.5f + 0.5f;
    if (previousUv.x < 0.0f || previousUv.x > 1.0f || previousUv.y < 0.0f || previousUv.y > 1.0f)
    {
        OutputImage[pixel] = currentColor;
        return;
    }

    float4 historyColor = HistoryImage.SampleLevel(LinearSampler, previousUv, 0.0f);
    float lumaDelta = abs(luminance(currentColor.rgb) - luminance(historyColor.rgb));
    float acceptance = 1.0f - smoothstep(0.08f, 0.35f, lumaDelta);
    float blendFactor = Params.splitAndGrid.w * acceptance;
    OutputImage[pixel] = float4(lerp(currentColor.rgb, historyColor.rgb, saturate01(blendFactor)), 1.0f);
}
