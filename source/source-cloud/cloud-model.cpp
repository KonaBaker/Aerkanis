#include "source-cloud/cloud-model.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_inverse.hpp>

namespace Aerkanis::Cloud
{

    namespace
    {

        auto finiteOr(float value, float fallback) noexcept -> float
        {
            return std::isfinite(value) ? value : fallback;
        }

        auto normalizeOr(glm::vec2 value, glm::vec2 fallback) -> glm::vec2
        {
            if (!std::isfinite(value.x) || !std::isfinite(value.y) || glm::length(value) < 0.001F)
            {
                return glm::normalize(fallback);
            }
            return glm::normalize(value);
        }

        auto matrixRow(glm::mat4 const& matrix, int rowIndex) -> glm::vec4
        {
            return glm::vec4{
                matrix[0][rowIndex],
                matrix[1][rowIndex],
                matrix[2][rowIndex],
                matrix[3][rowIndex],
            };
        }

    }  // namespace

    auto CloudSettings::sanitize() -> void
    {
        stepCount = std::clamp(stepCount, 8, 160);
        shadowStepCount = std::clamp(shadowStepCount, 0, 16);
        layerBottom = finiteOr(layerBottom, -0.35F);
        layerTop = finiteOr(layerTop, 1.85F);
        if (layerTop <= layerBottom + 0.05F)
        {
            layerTop = layerBottom + 0.05F;
        }

        coverageOffset = std::clamp(finiteOr(coverageOffset, -0.08F), -1.0F, 1.0F);
        coverageMultiplier = std::clamp(finiteOr(coverageMultiplier, 1.08F), 0.0F, 4.0F);
        densityMultiplier = std::clamp(finiteOr(densityMultiplier, 0.72F), 0.0F, 8.0F);
        extinction = std::clamp(finiteOr(extinction, 1.0F), 0.01F, 8.0F);
        absorption = std::clamp(finiteOr(absorption, 0.72F), 0.0F, 8.0F);
        weatherScale = std::clamp(finiteOr(weatherScale, 0.026F), 0.001F, 1.0F);
        shapeScale = std::clamp(finiteOr(shapeScale, 0.16F), 0.001F, 4.0F);
        detailScale = std::clamp(finiteOr(detailScale, 0.62F), 0.001F, 8.0F);
        detailStrength = std::clamp(finiteOr(detailStrength, 0.23F), 0.0F, 2.0F);
        windSpeed = std::clamp(finiteOr(windSpeed, 0.028F), -4.0F, 4.0F);
        typeBlend = std::clamp(finiteOr(typeBlend, 0.55F), 0.0F, 1.0F);
        phaseG = std::clamp(finiteOr(phaseG, 0.48F), -0.95F, 0.95F);
        powderStrength = std::clamp(finiteOr(powderStrength, 0.72F), 0.0F, 4.0F);
        baseBrightness = std::clamp(finiteOr(baseBrightness, 0.38F), 0.0F, 4.0F);
        exposure = std::clamp(finiteOr(exposure, 1.0F), 0.01F, 8.0F);

        windDirection = normalizeOr(windDirection, glm::vec2{1.0F, 0.28F});
    }

    auto makeCloudNubisParameters(
        CloudSettings const& settings,
        Environment::SunSkyState const& sunSky,
        Scene::Camera const& camera,
        vk::Extent2D extent,
        float elapsedSeconds) -> CloudNubisParameters
    {
        CloudSettings cloud = settings;
        cloud.sanitize();

        Scene::Camera sanitizedCamera = camera;
        sanitizedCamera.sanitize();

        const float aspectRatio =
            extent.height == 0
                ? 1.0F
                : static_cast<float>(extent.width) / static_cast<float>(extent.height);
        const glm::mat4 inverseViewProjection = glm::inverse(sanitizedCamera.viewProjection(aspectRatio));
        const glm::vec2 wind = cloud.windDirection * cloud.windSpeed;

        CloudNubisParameters parameters{
            .inverseViewProjectionRow0 = matrixRow(inverseViewProjection, 0),
            .inverseViewProjectionRow1 = matrixRow(inverseViewProjection, 1),
            .inverseViewProjectionRow2 = matrixRow(inverseViewProjection, 2),
            .inverseViewProjectionRow3 = matrixRow(inverseViewProjection, 3),
            .cameraPositionTime = glm::vec4{sanitizedCamera.position, std::max(elapsedSeconds, 0.0F)},
            .screenAndSteps = glm::vec4{
                static_cast<float>(extent.width),
                static_cast<float>(extent.height),
                cloud.enabled ? static_cast<float>(cloud.stepCount) : 0.0F,
                static_cast<float>(cloud.shadowStepCount),
            },
            .layerAndCoverage = glm::vec4{
                cloud.layerBottom,
                cloud.layerTop,
                cloud.coverageOffset,
                cloud.coverageMultiplier,
            },
            .windAndScale = glm::vec4{wind.x, wind.y, cloud.weatherScale, cloud.shapeScale},
            .detailAndDensity = glm::vec4{
                cloud.detailScale,
                cloud.detailStrength,
                cloud.densityMultiplier,
                cloud.extinction,
            },
            .phaseAndPowder = glm::vec4{
                cloud.phaseG,
                cloud.powderStrength,
                cloud.typeBlend,
                cloud.baseBrightness,
            },
            .sunDirectionExposure = glm::vec4{glm::normalize(sunSky.sunDirection), cloud.exposure},
            .sunColorIntensity = glm::vec4{sunSky.sunColor, sunSky.sunIntensity},
            .ambientColorAbsorption = glm::vec4{sunSky.ambientColor, cloud.absorption},
            .gradientStratus = glm::vec4{0.00F, 0.12F, 0.54F, 0.86F},
            .gradientCumulus = glm::vec4{0.02F, 0.22F, 0.72F, 1.00F},
            .gradientCumulonimbus = glm::vec4{0.00F, 0.14F, 0.92F, 1.00F},
            .skyHorizonColor = glm::vec4{
                sunSky.skyHorizonColor * sunSky.skyIntensity,
                sunSky.sunAngularRadiusRadians,
            },
            .skyZenithColor = glm::vec4{
                sunSky.skyZenithColor * sunSky.skyIntensity,
                sunSky.sunGlowStrength,
            },
        };
        return parameters;
    }

}  // namespace Aerkanis::Cloud
