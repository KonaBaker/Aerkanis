struct CloudNubisCubedParameters
{
    float4 inverseViewProjectionRow0;
    float4 inverseViewProjectionRow1;
    float4 inverseViewProjectionRow2;
    float4 inverseViewProjectionRow3;
    float4 cameraPositionTime;
    float4 screenAndSteps;
    float4 boundsMinDataset;
    float4 boundsMaxDensity;
    float4 detailAndMarch;
    float4 windAndExposure;
    float4 lighting;
    float4 sunDirectionAbsorption;
    float4 sunColorIntensity;
    float4 ambientColor;
    float4 skyHorizonColor;
    float4 skyZenithColor;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CloudNubisCubedParameters> Params;
[[vk::binding(1, 0)]]
RWTexture2D<float4> OutputImage;
[[vk::binding(2, 0)]]
Texture3D<float4> ModelingVolume0;
[[vk::binding(3, 0)]]
Texture3D<float4> ModelingVolume1;
[[vk::binding(4, 0)]]
Texture3D<float4> DetailNoise;
[[vk::binding(5, 0)]]
SamplerState LinearSampler;

static const float Pi = 3.14159265f;

struct DensitySample
{
    float profile;
    float full;
    float lowLod;
    float sdf;
};

float saturate01(float value)
{
    return clamp(value, 0.0f, 1.0f);
}

float3 saturate03(float3 value)
{
    return clamp(value, float3(0.0f), float3(1.0f));
}

float remap(float value, float oldMin, float oldMax, float newMin, float newMax)
{
    float denominator = max(oldMax - oldMin, 0.0001f);
    return newMin + saturate01((value - oldMin) / denominator) * (newMax - newMin);
}

float valueErosion(float value, float oldMin)
{
    float denominator = max(1.0f - oldMin, 0.0001f);
    return saturate01((value - oldMin) / denominator);
}

float4 applyInverseViewProjection(float4 position)
{
    return float4(
        dot(Params.inverseViewProjectionRow0, position),
        dot(Params.inverseViewProjectionRow1, position),
        dot(Params.inverseViewProjectionRow2, position),
        dot(Params.inverseViewProjectionRow3, position));
}

float3 rayDirectionFromPixel(uint2 pixel)
{
    float2 resolution = max(Params.screenAndSteps.xy, float2(1.0f, 1.0f));
    float2 uv = (float2(pixel) + 0.5f) / resolution;
    float2 ndc = uv * 2.0f - 1.0f;
    float4 worldNear = applyInverseViewProjection(float4(ndc, 0.0f, 1.0f));
    float4 worldFar = applyInverseViewProjection(float4(ndc, 1.0f, 1.0f));
    worldNear.xyz /= max(worldNear.w, 0.0001f);
    worldFar.xyz /= max(worldFar.w, 0.0001f);
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

bool insideBounds(float3 position)
{
    float3 boundsMin = Params.boundsMinDataset.xyz;
    float3 boundsMax = Params.boundsMaxDensity.xyz;
    return position.x >= boundsMin.x && position.x <= boundsMax.x &&
        position.y >= boundsMin.y && position.y <= boundsMax.y &&
        position.z >= boundsMin.z && position.z <= boundsMax.z;
}

float3 volumeCoord(float3 worldPosition)
{
    float3 boundsMin = Params.boundsMinDataset.xyz;
    float3 boundsMax = Params.boundsMaxDensity.xyz;
    float3 normalizedWorld = saturate03((worldPosition - boundsMin) / max(boundsMax - boundsMin, float3(0.001f)));
    return float3(1.0f - normalizedWorld.x, 1.0f - normalizedWorld.z, normalizedWorld.y);
}

float4 sampleModel(float3 coord)
{
    if (Params.boundsMinDataset.w < 0.5f)
    {
        return ModelingVolume0.SampleLevel(LinearSampler, coord, 0.0f);
    }
    return ModelingVolume1.SampleLevel(LinearSampler, coord, 0.0f);
}

float signedDistanceFromAlpha(float alpha)
{
    float horizontalSpan = max(Params.boundsMaxDensity.x - Params.boundsMinDataset.x, 0.001f);
    return lerp(-0.125f * horizontalSpan, 2.0f * horizontalSpan, alpha);
}

float henyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denominator = pow(max(1.0f + g2 - 2.0f * g * cosTheta, 0.001f), 1.5f);
    return (1.0f - g2) / max(4.0f * Pi * denominator, 0.001f);
}

float hashPixel(uint2 pixel)
{
    float2 p = float2(pixel);
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

DensitySample sampleDensity(float3 worldPosition, bool cheap, float rayDistance)
{
    DensitySample result;
    result.profile = 0.0f;
    result.full = 0.0f;
    result.lowLod = 0.0f;
    result.sdf = 0.0f;

    if (!insideBounds(worldPosition))
    {
        return result;
    }

    float3 coord = volumeCoord(worldPosition);
    float4 model = sampleModel(coord);
    float profile = saturate01(model.r);
    float detailType = saturate01(model.g);
    float densityScale = saturate01(model.b);
    float sdf = signedDistanceFromAlpha(model.a);

    result.profile = profile * densityScale;
    result.lowLod = result.profile;
    result.sdf = sdf;

    if (sdf >= 0.0f || profile <= 0.001f || densityScale <= 0.001f)
    {
        return result;
    }

    if (cheap)
    {
        result.full = result.lowLod * Params.boundsMaxDensity.w;
        return result;
    }

    float time = Params.cameraPositionTime.w;
    float2 wind = Params.windAndExposure.xy * time;
    float detailScale = Params.detailAndMarch.x;
    float detailStrength = Params.detailAndMarch.y;
    float3 detailCoord = coord * detailScale + float3(wind.x, wind.y * 0.35f, wind.y);
    float4 noise = DetailNoise.SampleLevel(LinearSampler, frac(detailCoord), 0.0f);

    float wispyNoise = lerp(noise.r, noise.g, profile);
    float billowyTypeGradient = pow(max(profile, 0.001f), 0.25f);
    float billowyNoise = lerp(noise.b * 0.32f, noise.a * 0.32f, billowyTypeGradient);
    float erosion = lerp(wispyNoise, billowyNoise, detailType);

    float highWisps = 1.0f - pow(abs(abs(noise.g * 2.0f - 1.0f) * 2.0f - 1.0f), 4.0f);
    float highBillows = pow(abs(abs(noise.a * 2.0f - 1.0f) * 2.0f - 1.0f), 2.0f);
    float highErosion = saturate01(lerp(highWisps, highBillows, detailType));
    erosion = lerp(erosion, highErosion, saturate01(detailStrength * 0.35f));

    float uprezzedDensity = valueErosion(profile, erosion * detailStrength);
    float poweredDensityScale = pow(densityScale, 3.0f);
    uprezzedDensity *= poweredDensityScale;
    uprezzedDensity = pow(max(uprezzedDensity, 0.0f), lerp(0.34f, 0.68f, poweredDensityScale));

    float distanceBlend = remap(rayDistance, 1.0f, max(Params.detailAndMarch.z * 0.65f, 1.0f), 0.52f, 1.0f);
    float sdfMask = saturate01(1.0f - max(sdf, 0.0f) / max(0.34f * Params.lighting.w, 0.001f));
    result.full = uprezzedDensity *
        lerp(0.68f, 1.0f, sdfMask) *
        distanceBlend *
        Params.boundsMaxDensity.w;

    return result;
}

float densityToSun(float3 position, float viewStepLength)
{
    uint shadowSteps = (uint)Params.screenAndSteps.w;
    if (shadowSteps == 0)
    {
        return 0.0f;
    }

    float3 sunDirection = normalize(Params.sunDirectionAbsorption.xyz);
    float3 boundsSize = max(Params.boundsMaxDensity.xyz - Params.boundsMinDataset.xyz, float3(0.001f));
    float stepLength = max(viewStepLength * 1.35f, length(boundsSize) * 0.42f / max((float)shadowSteps, 1.0f));
    float opticalDepth = 0.0f;

    for (uint stepIndex = 0; stepIndex < shadowSteps; ++stepIndex)
    {
        float3 samplePosition = position + sunDirection * (float(stepIndex) + 0.5f) * stepLength;
        if (!insideBounds(samplePosition))
        {
            break;
        }

        opticalDepth += sampleDensity(samplePosition, true, 0.0f).full * stepLength;
    }

    return opticalDepth;
}

float3 skyColor(float3 direction)
{
    float horizon = saturate01(direction.y * 0.5f + 0.5f);
    float3 sky = lerp(Params.skyHorizonColor.rgb, Params.skyZenithColor.rgb, pow(horizon, 0.68f));
    float3 sunDirection = normalize(Params.sunDirectionAbsorption.xyz);
    float sunCos = dot(normalize(direction), sunDirection);
    float sunRadius = max(Params.skyHorizonColor.w, 0.0001f);
    float sunDisc = smoothstep(cos(sunRadius * 2.0f), cos(sunRadius), sunCos);
    float sunGlow = pow(saturate01(sunCos), 96.0f) * Params.skyZenithColor.w;
    float horizonGlow = pow(1.0f - horizon, 2.0f) * max(sunDirection.y, 0.0f) * 0.18f;
    float3 sun = Params.sunColorIntensity.rgb * Params.sunColorIntensity.w;
    return sky + sun * (sunDisc + sunGlow * 0.22f + horizonGlow);
}

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

    float3 cameraPosition = Params.cameraPositionTime.xyz;
    float3 rayDirection = rayDirectionFromPixel(pixel);
    float3 background = skyColor(rayDirection);

    float entryDistance = 0.0f;
    float exitDistance = 0.0f;
    uint stepCount = (uint)Params.screenAndSteps.z;
    if (stepCount == 0 || !intersectBounds(cameraPosition, rayDirection, entryDistance, exitDistance))
    {
        float3 sky = 1.0f - exp(-background * Params.windAndExposure.w);
        OutputImage[pixel] = float4(sky, 1.0f);
        return;
    }

    float travel = max(exitDistance - entryDistance, 0.0f);
    if (travel <= 0.001f)
    {
        float3 sky = 1.0f - exp(-background * Params.windAndExposure.w);
        OutputImage[pixel] = float4(sky, 1.0f);
        return;
    }

    float baseStepLength = travel / max((float)stepCount, 1.0f);
    float distance = entryDistance + hashPixel(pixel) * baseStepLength;
    float transmittance = 1.0f;
    float3 scattering = 0.0f;
    float3 sunDirection = normalize(Params.sunDirectionAbsorption.xyz);
    float sunElevation = saturate01(sunDirection.y * 0.5f + 0.5f);
    float cosTheta = dot(rayDirection, sunDirection);
    float phase = henyeyGreenstein(cosTheta, Params.lighting.x);
    float extinction = max(Params.sunDirectionAbsorption.w, 0.001f);

    for (uint stepIndex = 0;
         stepIndex < stepCount && distance < exitDistance && transmittance > Params.detailAndMarch.w;
         ++stepIndex)
    {
        float3 position = cameraPosition + rayDirection * distance;
        DensitySample sample = sampleDensity(position, false, distance);

        if (sample.full <= 0.001f)
        {
            float skipLength = sample.sdf > 0.0f
                ? clamp(sample.sdf * 0.42f, baseStepLength, baseStepLength * 5.0f)
                : baseStepLength;
            distance += skipLength;
            continue;
        }

        float opticalDepth = sample.full * extinction * baseStepLength;
        float sampleTransmittance = exp(-opticalDepth);
        float sunOpticalDepth = densityToSun(position, baseStepLength);
        float lightTransmittance = exp(-sunOpticalDepth * extinction);
        float powder = 1.0f - exp(-sample.full * Params.lighting.y * 2.0f);
        float silverLining = pow(saturate01(cosTheta), 12.0f) * 0.32f;
        float3 lighting =
            Params.ambientColor.rgb * Params.lighting.z +
            Params.sunColorIntensity.rgb *
                Params.sunColorIntensity.w *
                lightTransmittance *
                (phase * lerp(4.8f, 8.4f, sunElevation) + 0.13f + silverLining);
        lighting = lerp(lighting, lighting * powder, 0.34f);

        scattering += transmittance * (1.0f - sampleTransmittance) * lighting;
        transmittance *= sampleTransmittance;
        distance += baseStepLength;
    }

    float3 composite = background * transmittance + scattering;
    composite = 1.0f - exp(-composite * Params.windAndExposure.w);
    OutputImage[pixel] = float4(composite, 1.0f);
}
