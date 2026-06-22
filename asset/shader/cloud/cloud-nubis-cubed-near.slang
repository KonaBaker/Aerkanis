#include "cloud-nubis-cubed-params.slangh"

[[vk::binding(0, 0)]]
ConstantBuffer<CloudNubisCubedParameters> Params;
[[vk::binding(1, 0)]]
RWTexture2D<float4> NearStateImage;
[[vk::binding(2, 0)]]
Texture3D<float4> ModelingVolume0;
[[vk::binding(3, 0)]]
Texture3D<float4> ModelingVolume1;
[[vk::binding(4, 0)]]
Texture3D<float4> DetailNoise;
[[vk::binding(5, 0)]]
Texture3D<float4> LightGrid;
[[vk::binding(6, 0)]]
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

    MarchState state;
    state.scattering = float3(0.0f);
    state.transmittance = 1.0f;
    state.density = 0.0f;
    state.firstHitDistance = -1.0f;

    uint stepCount = (uint)Params.screenAndSteps.z;
    if (stepCount == 0 || Params.featureFlags.y <= 0.5f)
    {
        NearStateImage[pixel] = float4(state.scattering, state.transmittance);
        return;
    }

    float3 cameraPosition = Params.cameraPositionTime.xyz;
    float3 rayDirection = rayDirectionFromPixel(pixel);
    float entryDistance = 0.0f;
    float exitDistance = 0.0f;
    if (!intersectBounds(cameraPosition, rayDirection, entryDistance, exitDistance))
    {
        NearStateImage[pixel] = float4(state.scattering, state.transmittance);
        return;
    }

    float nearEnd = min(exitDistance, Params.splitAndGrid.x);
    uint nearSteps = max(stepCount / 2U, 8U);
    marchCloudSegment(pixel, cameraPosition, rayDirection, entryDistance, nearEnd, nearSteps, state);

    NearStateImage[pixel] = float4(state.scattering, state.transmittance);
}
