struct CloudNubisParameters
{
    float4 inverseViewProjectionRow0;
    float4 inverseViewProjectionRow1;
    float4 inverseViewProjectionRow2;
    float4 inverseViewProjectionRow3;
    float4 cameraPositionTime;
    float4 screenAndSteps;
    float4 layerAndCoverage;
    float4 windAndScale;
    float4 detailAndDensity;
    float4 phaseAndPowder;
    float4 sunDirectionExposure;
    float4 sunColorIntensity;
    float4 ambientColorAbsorption;
    float4 gradientStratus;
    float4 gradientCumulus;
    float4 gradientCumulonimbus;
    float4 skyHorizonColor;
    float4 skyZenithColor;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CloudNubisParameters> Params;
[[vk::binding(1, 0)]]
RWTexture2D<float4> OutputImage;
[[vk::binding(2, 0)]]
Texture2D<float4> WeatherMap;
[[vk::binding(3, 0)]]
Texture2D<float4> CurlNoise;
[[vk::binding(4, 0)]]
Texture3D<float4> ShapeNoise;
[[vk::binding(5, 0)]]
Texture3D<float4> DetailNoise;
[[vk::binding(6, 0)]]
Texture3D<float4> LowModelVolume;
[[vk::binding(7, 0)]]
Texture3D<float4> HighModelVolume;
[[vk::binding(8, 0)]]
SamplerState LinearSampler;

float saturate01(float value)
{
    return clamp(value, 0.0f, 1.0f);
}

float remap(float value, float oldMin, float oldMax, float newMin, float newMax)
{
    float denominator = max(oldMax - oldMin, 0.0001f);
    return newMin + saturate01((value - oldMin) / denominator) * (newMax - newMin);
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

bool intersectLayer(float3 origin, float3 direction, out float entryDistance, out float exitDistance)
{
    float bottom = Params.layerAndCoverage.x;
    float top = Params.layerAndCoverage.y;
    if (abs(direction.y) < 0.0001f)
    {
        if (origin.y < bottom || origin.y > top)
        {
            entryDistance = 0.0f;
            exitDistance = 0.0f;
            return false;
        }

        entryDistance = 0.0f;
        exitDistance = 80.0f;
        return true;
    }

    float t0 = (bottom - origin.y) / direction.y;
    float t1 = (top - origin.y) / direction.y;
    entryDistance = max(min(t0, t1), 0.0f);
    exitDistance = max(t0, t1);
    return exitDistance > entryDistance;
}

float heightFraction(float y)
{
    return saturate01((y - Params.layerAndCoverage.x) / max(Params.layerAndCoverage.y - Params.layerAndCoverage.x, 0.001f));
}

float nubis1HeightGradient(float height, float4 gradient)
{
    float bottomFade = saturate01((height - gradient.x) / max(gradient.y - gradient.x, 0.0001f));
    float topFade = saturate01((gradient.w - height) / max(gradient.w - gradient.z, 0.0001f));
    return saturate01(bottomFade * topFade);
}

float nubis2VerticalProfile(float height, float weatherType)
{
    float stratus = nubis1HeightGradient(height, Params.gradientStratus);
    float cumulus = nubis1HeightGradient(height, Params.gradientCumulus);
    float cumulonimbus = nubis1HeightGradient(height, Params.gradientCumulonimbus);
    float lowToTall = lerp(stratus, cumulus, saturate01(weatherType * 2.0f));
    float tallBlend = saturate01((weatherType - 0.5f) * 2.0f);
    return lerp(lowToTall, cumulonimbus, tallBlend);
}

float henyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    float denominator = pow(max(1.0f + g2 - 2.0f * g * cosTheta, 0.001f), 1.5f);
    return (1.0f - g2) / max(4.0f * 3.14159265f * denominator, 0.001f);
}

float sampleDensity(float3 worldPosition, bool cheap)
{
    float height = heightFraction(worldPosition.y);
    if (height <= 0.0f || height >= 1.0f)
    {
        return 0.0f;
    }

    float time = Params.cameraPositionTime.w;
    float2 wind = Params.windAndScale.xy * time;
    float weatherScale = Params.windAndScale.z;
    float shapeScale = Params.windAndScale.w;
    float detailScale = Params.detailAndDensity.x;
    float detailStrength = Params.detailAndDensity.y;
    float coverageOffset = Params.layerAndCoverage.z;
    float coverageMultiplier = Params.layerAndCoverage.w;
    float typeBlend = Params.phaseAndPowder.z;

    float2 weatherUv = worldPosition.xz * weatherScale + wind;
    float4 weather = WeatherMap.SampleLevel(LinearSampler, weatherUv, 0.0f);
    float coverage = saturate01(weather.r * coverageMultiplier + coverageOffset);
    float weatherType = saturate01(lerp(weather.g, weather.b, typeBlend));
    float profile = nubis2VerticalProfile(height, weatherType);
    if (coverage <= 0.001f || profile <= 0.001f)
    {
        return 0.0f;
    }

    float3 shapeUv = worldPosition * shapeScale + float3(wind.x, 0.0f, wind.y);
    float4 shape = ShapeNoise.SampleLevel(LinearSampler, shapeUv, 0.0f);
    float4 model = LowModelVolume.SampleLevel(LinearSampler, float3(frac(shapeUv.xy), height), 0.0f);
    float baseShape = saturate01(shape.r * 0.58f + shape.g * 0.26f + model.r * 0.28f - shape.b * 0.12f);
    float density = remap(baseShape * profile, 1.0f - coverage, 1.0f, 0.0f, 1.0f);

    if (!cheap && density > 0.001f)
    {
        float2 curl = CurlNoise.SampleLevel(LinearSampler, weatherUv * 2.5f + height, 0.0f).rg * 2.0f - 1.0f;
        float3 detailUv = worldPosition * detailScale + float3(curl.x, height * 0.35f, curl.y);
        float4 detail = DetailNoise.SampleLevel(LinearSampler, detailUv, 0.0f);
        float4 high = HighModelVolume.SampleLevel(LinearSampler, float3(frac(detailUv.xy), height), 0.0f);
        float erosion = detail.r * 0.52f + detail.g * 0.23f + high.r * 0.32f;
        density = saturate01(density - erosion * detailStrength * (1.0f - height * 0.45f));
    }

    return density * Params.detailAndDensity.z;
}

float lightTransmittance(float3 position, float density)
{
    uint shadowSteps = (uint)Params.screenAndSteps.w;
    if (shadowSteps == 0)
    {
        return 1.0f;
    }

    float3 sunDirection = normalize(Params.sunDirectionExposure.xyz);
    float bottom = Params.layerAndCoverage.x;
    float top = Params.layerAndCoverage.y;
    float exitDistance = sunDirection.y > 0.0f
        ? (top - position.y) / max(sunDirection.y, 0.001f)
        : (bottom - position.y) / min(sunDirection.y, -0.001f);
    exitDistance = clamp(exitDistance, 0.02f, 12.0f);

    float stepLength = exitDistance / max((float)shadowSteps, 1.0f);
    float opticalDepth = 0.0f;
    for (uint i = 0; i < shadowSteps; ++i)
    {
        float t = (float(i) + 0.5f) * stepLength;
        opticalDepth += sampleDensity(position + sunDirection * t, true) * stepLength;
    }

    float extinction = Params.detailAndDensity.w;
    float powder = 1.0f - exp(-density * Params.phaseAndPowder.y * 2.0f);
    return exp(-opticalDepth * extinction * Params.ambientColorAbsorption.w) * lerp(1.0f, powder, 0.65f);
}

float3 skyColor(float3 direction)
{
    float horizon = saturate01(direction.y * 0.5f + 0.5f);
    float3 sky = lerp(Params.skyHorizonColor.rgb, Params.skyZenithColor.rgb, horizon);
    float3 sunDirection = normalize(Params.sunDirectionExposure.xyz);
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
    float3 color = skyColor(rayDirection);

    float entryDistance = 0.0f;
    float exitDistance = 0.0f;
    uint stepCount = (uint)Params.screenAndSteps.z;
    if (stepCount == 0 || !intersectLayer(cameraPosition, rayDirection, entryDistance, exitDistance))
    {
        OutputImage[pixel] = float4(color, 1.0f);
        return;
    }

    exitDistance = min(exitDistance, 80.0f);
    float travel = max(exitDistance - entryDistance, 0.0f);
    if (travel <= 0.001f)
    {
        OutputImage[pixel] = float4(color, 1.0f);
        return;
    }

    float stepLength = travel / max((float)stepCount, 1.0f);
    float transmittance = 1.0f;
    float3 scattering = 0.0f;
    float3 sunDirection = normalize(Params.sunDirectionExposure.xyz);
    float sunElevation = saturate01(sunDirection.y * 0.5f + 0.5f);
    float phase = henyeyGreenstein(dot(rayDirection, sunDirection), Params.phaseAndPowder.x);
    float extinction = Params.detailAndDensity.w;

    for (uint i = 0; i < stepCount; ++i)
    {
        float t = entryDistance + (float(i) + 0.5f) * stepLength;
        float3 position = cameraPosition + rayDirection * t;
        float density = sampleDensity(position, false);
        if (density <= 0.001f)
        {
            continue;
        }

        float opticalDepth = density * extinction * stepLength;
        float sampleTransmittance = exp(-opticalDepth);
        float light = lightTransmittance(position, density);
        float silverLining = pow(saturate01(dot(rayDirection, sunDirection)), 12.0f) * 0.35f;
        float3 lighting =
            Params.ambientColorAbsorption.rgb * Params.phaseAndPowder.w +
            Params.sunColorIntensity.rgb *
                Params.sunColorIntensity.w *
                light *
                (phase * lerp(5.0f, 8.0f, sunElevation) + 0.14f + silverLining);
        scattering += transmittance * (1.0f - sampleTransmittance) * lighting;
        transmittance *= sampleTransmittance;
        if (transmittance < 0.01f)
        {
            break;
        }
    }

    float3 composite = color * transmittance + scattering;
    composite = 1.0f - exp(-composite * Params.sunDirectionExposure.w);
    OutputImage[pixel] = float4(composite, 1.0f);
}
