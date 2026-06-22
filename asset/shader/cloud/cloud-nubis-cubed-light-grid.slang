#include "cloud-nubis-cubed-params.slangh"

[[vk::binding(0, 0)]]
ConstantBuffer<CloudNubisCubedParameters> Params;
[[vk::binding(1, 0)]]
RWTexture3D<float4> LightGridImage;
[[vk::binding(2, 0)]]
Texture3D<float4> ModelingVolume0;
[[vk::binding(3, 0)]]
Texture3D<float4> ModelingVolume1;
[[vk::binding(4, 0)]]
SamplerState LinearSampler;

float saturate01(float value)
{
    return clamp(value, 0.0f, 1.0f);
}

float3 saturate03(float3 value)
{
    return clamp(value, float3(0.0f), float3(1.0f));
}

float3 volumeCoord(float3 worldPosition)
{
    float3 boundsMin = Params.boundsMinDataset.xyz;
    float3 boundsMax = Params.boundsMaxDensity.xyz;
    float3 normalizedWorld = saturate03((worldPosition - boundsMin) / max(boundsMax - boundsMin, float3(0.001f)));
    return float3(1.0f - normalizedWorld.x, 1.0f - normalizedWorld.z, normalizedWorld.y);
}

float3 worldPositionFromVolumeCoord(float3 coord)
{
    float3 normalizedWorld = float3(1.0f - coord.x, coord.z, 1.0f - coord.y);
    return Params.boundsMinDataset.xyz +
        normalizedWorld * max(Params.boundsMaxDensity.xyz - Params.boundsMinDataset.xyz, float3(0.001f));
}

bool insideBounds(float3 position)
{
    float3 boundsMin = Params.boundsMinDataset.xyz;
    float3 boundsMax = Params.boundsMaxDensity.xyz;
    return position.x >= boundsMin.x && position.x <= boundsMax.x &&
        position.y >= boundsMin.y && position.y <= boundsMax.y &&
        position.z >= boundsMin.z && position.z <= boundsMax.z;
}

float signedDistanceFromAlpha(float alpha)
{
    float horizontalSpan = max(Params.boundsMaxDensity.x - Params.boundsMinDataset.x, 0.001f);
    return lerp(-0.125f * horizontalSpan, 2.0f * horizontalSpan, alpha);
}

float4 sampleModel(float3 coord)
{
    if (Params.boundsMinDataset.w < 0.5f)
    {
        return ModelingVolume0.SampleLevel(LinearSampler, coord, 0.0f);
    }
    return ModelingVolume1.SampleLevel(LinearSampler, coord, 0.0f);
}

float profileDensityAtCoord(float3 coord)
{
    float4 model = sampleModel(saturate03(coord));
    float profile = saturate01(model.r);
    float densityScale = saturate01(model.b);
    float sdf = signedDistanceFromAlpha(model.a);
    if (profile <= 0.001f || densityScale <= 0.001f || sdf >= 0.0f)
    {
        return 0.0f;
    }
    return profile * densityScale * Params.boundsMaxDensity.w;
}

[shader("compute")]
[numthreads(4, 4, 4)]
void compMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint gridXY = (uint)max(Params.splitAndGrid.y, 1.0f);
    uint gridZ = (uint)max(Params.splitAndGrid.z, 1.0f);
    uint3 coord = dispatchThreadId;
    if (coord.x >= gridXY || coord.y >= gridXY || coord.z >= gridZ)
    {
        return;
    }

    float3 gridSize = float3((float)gridXY, (float)gridXY, (float)gridZ);
    float3 volumePosition = (float3(coord) + 0.5f) / gridSize;
    float density = profileDensityAtCoord(volumePosition);

    if (Params.featureFlags.x <= 0.5f || density <= 0.0f)
    {
        LightGridImage[coord] = float4(0.0f, density, 0.0f, 1.0f);
        return;
    }

    float3 boundsSize = max(Params.boundsMaxDensity.xyz - Params.boundsMinDataset.xyz, float3(0.001f));
    float horizontalCell = max(boundsSize.x, boundsSize.z) / max((float)gridXY, 1.0f);
    float verticalCell = boundsSize.y / max((float)gridZ, 1.0f);
    float stepLength = max(max(horizontalCell, verticalCell), 0.001f);
    uint stepCount = min(max((uint)Params.screenAndSteps.w * 4U, 8U), 48U);
    float3 sunDirection = normalize(Params.sunDirectionAbsorption.xyz);
    float3 worldPosition = worldPositionFromVolumeCoord(volumePosition);
    float opticalDepth = 0.0f;

    for (uint stepIndex = 1; stepIndex <= stepCount; ++stepIndex)
    {
        float3 samplePosition = worldPosition + sunDirection * ((float)stepIndex + 0.5f) * stepLength;
        if (!insideBounds(samplePosition))
        {
            break;
        }
        opticalDepth += profileDensityAtCoord(volumeCoord(samplePosition)) * stepLength;
    }

    LightGridImage[coord] = float4(opticalDepth, density, 0.0f, 1.0f);
}
