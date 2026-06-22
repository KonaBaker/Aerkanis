#include "cloud-nubis-cubed-params.slangh"

[[vk::binding(0, 0)]]
ConstantBuffer<CloudNubisCubedParameters> Params;
[[vk::binding(1, 0)]]
RWTexture2D<float4> RawOutputImage;
[[vk::binding(2, 0)]]
Texture2D<float4> NearStateImage;
[[vk::binding(3, 0)]]
Texture3D<float4> ModelingVolume0;
[[vk::binding(4, 0)]]
Texture3D<float4> ModelingVolume1;
[[vk::binding(5, 0)]]
Texture3D<float4> DetailNoise;
[[vk::binding(6, 0)]]
Texture3D<float4> LightGrid;
[[vk::binding(7, 0)]]
SamplerState LinearSampler;

#include "cloud-nubis-cubed-march.slangh"

[shader("compute")]
[numthreads(8, 8, 1)]
void compMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadId.xy;
    uint2 extent = uint2(
        (uint)max(Params.screenAndSteps.x, 1.0f),
        (uint)max(Params.screenAndSteps.y, 1.0f));
    if (pixel.x >= extent.x || pixel.y >= extent.y)
    {
        return;
    }

    float2 uv = (float2(pixel) + 0.5f) / max(Params.screenAndSteps.xy, float2(1.0f, 1.0f));
    float4 nearState = Params.featureFlags.y > 0.5f
        ? NearStateImage.SampleLevel(LinearSampler, uv, 0.0f)
        : float4(0.0f, 0.0f, 0.0f, 1.0f);

    MarchState state;
    state.scattering = nearState.rgb;
    state.transmittance = nearState.a;
    state.density = 0.0f;
    state.firstHitDistance = -1.0f;

    float3 cameraPosition = Params.cameraPositionTime.xyz;
    float3 rayDirection = rayDirectionFromPixel(pixel);
    float3 background = skyColor(rayDirection);

    float entryDistance = 0.0f;
    float exitDistance = 0.0f;
    uint stepCount = (uint)Params.screenAndSteps.z;
    if (stepCount != 0 && intersectBounds(cameraPosition, rayDirection, entryDistance, exitDistance))
    {
        bool splitEnabled = Params.featureFlags.y > 0.5f;
        float startDistance = splitEnabled ? max(entryDistance, min(exitDistance, Params.splitAndGrid.x)) : entryDistance;
        uint farSteps = splitEnabled ? max(stepCount - stepCount / 2U, 8U) : stepCount;
        marchCloudSegment(pixel, cameraPosition, rayDirection, startDistance, exitDistance, farSteps, state);
    }

    float3 composite = background * state.transmittance + state.scattering;
    composite = 1.0f - exp(-composite * Params.windAndExposure.w);
    RawOutputImage[pixel] = float4(composite, 1.0f);
}
