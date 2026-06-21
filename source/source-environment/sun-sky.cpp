#include "source-environment/sun-sky.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace Aerkanis::Environment
{

    namespace
    {

        auto finiteOr(float value, float fallback) noexcept -> float
        {
            return std::isfinite(value) ? value : fallback;
        }

        auto saturate(float value) noexcept -> float
        {
            return std::clamp(value, 0.0F, 1.0F);
        }

        auto smoothStep(float edge0, float edge1, float value) noexcept -> float
        {
            const float denominator = std::max(edge1 - edge0, 0.0001F);
            const float t = saturate((value - edge0) / denominator);
            return t * t * (3.0F - 2.0F * t);
        }

        auto sanitizeColor(glm::vec3 color, glm::vec3 fallback) -> glm::vec3
        {
            if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z))
            {
                return fallback;
            }
            return glm::clamp(color, glm::vec3{0.0F}, glm::vec3{8.0F});
        }

        auto wrapDegrees(float degrees) noexcept -> float
        {
            float wrapped = std::fmod(degrees, 360.0F);
            if (wrapped > 180.0F)
            {
                wrapped -= 360.0F;
            }
            if (wrapped < -180.0F)
            {
                wrapped += 360.0F;
            }
            return wrapped;
        }

        auto directionFromAngles(float azimuthDegrees, float elevationDegrees) -> glm::vec3
        {
            const float azimuth = glm::radians(azimuthDegrees);
            const float elevation = glm::radians(elevationDegrees);
            const float horizontal = std::cos(elevation);
            return glm::normalize(glm::vec3{
                horizontal * std::cos(azimuth),
                std::sin(elevation),
                horizontal * std::sin(azimuth),
            });
        }

    }  // namespace

    auto SunSkySettings::sanitize() -> void
    {
        sunAzimuthDegrees = wrapDegrees(finiteOr(sunAzimuthDegrees, -122.0F));
        sunElevationDegrees = std::clamp(finiteOr(sunElevationDegrees, 38.0F), -12.0F, 89.0F);
        sunIntensity = std::clamp(finiteOr(sunIntensity, 3.8F), 0.0F, 32.0F);
        ambientIntensity = std::clamp(finiteOr(ambientIntensity, 0.72F), 0.0F, 8.0F);
        skyIntensity = std::clamp(finiteOr(skyIntensity, 1.0F), 0.0F, 8.0F);
        sunAngularRadiusDegrees = std::clamp(finiteOr(sunAngularRadiusDegrees, 0.53F), 0.02F, 5.0F);
        sunGlowStrength = std::clamp(finiteOr(sunGlowStrength, 0.18F), 0.0F, 8.0F);
        sunTint = sanitizeColor(sunTint, glm::vec3{1.0F, 0.88F, 0.66F});
        horizonTint = sanitizeColor(horizonTint, glm::vec3{0.54F, 0.68F, 0.86F});
        zenithTint = sanitizeColor(zenithTint, glm::vec3{0.08F, 0.18F, 0.38F});
        groundTint = sanitizeColor(groundTint, glm::vec3{0.015F, 0.018F, 0.026F});
    }

    auto makeSunSkyState(SunSkySettings const& settings) -> SunSkyState
    {
        SunSkySettings sky = settings;
        sky.sanitize();

        const float daylight = sky.enabled ? smoothStep(-6.0F, 10.0F, sky.sunElevationDegrees) : 0.0F;
        const float lowSunWarmth =
            smoothStep(-4.0F, 8.0F, sky.sunElevationDegrees) *
            (1.0F - smoothStep(18.0F, 42.0F, sky.sunElevationDegrees));
        const glm::vec3 sunsetTint{1.0F, 0.45F, 0.22F};
        const glm::vec3 sunColor = glm::mix(sky.sunTint * sunsetTint, sky.sunTint, daylight);
        const glm::vec3 nightHorizon{0.018F, 0.026F, 0.052F};
        const glm::vec3 nightZenith{0.004F, 0.008F, 0.022F};
        const glm::vec3 horizonColor =
            glm::mix(nightHorizon, sky.horizonTint + sunsetTint * lowSunWarmth * 0.28F, daylight);
        const glm::vec3 zenithColor = glm::mix(nightZenith, sky.zenithTint, daylight);
        const glm::vec3 ambientColor =
            (horizonColor * 0.58F + zenithColor * 0.42F) *
            sky.ambientIntensity *
            (0.18F + daylight * 0.82F);

        return SunSkyState{
            .sunDirection = directionFromAngles(sky.sunAzimuthDegrees, sky.sunElevationDegrees),
            .sunIntensity = sky.sunIntensity * daylight,
            .sunColor = sunColor,
            .daylight = daylight,
            .ambientColor = ambientColor,
            .skyIntensity = sky.skyIntensity,
            .skyHorizonColor = horizonColor,
            .sunAngularRadiusRadians = sky.sunAngularRadiusDegrees * std::numbers::pi_v<float> / 180.0F,
            .skyZenithColor = zenithColor,
            .sunGlowStrength = sky.sunGlowStrength * daylight,
            .skyGroundColor = sky.groundTint,
            .padding = 0.0F,
        };
    }

}  // namespace Aerkanis::Environment
