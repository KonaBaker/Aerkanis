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

        auto directionFromAngle(float elevationDegrees) -> glm::vec3
        {
            constexpr float fixedAzimuthDegrees = -122.0F;
            const float azimuth = glm::radians(fixedAzimuthDegrees);
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
        sunAngleDegrees = std::clamp(finiteOr(sunAngleDegrees, 38.0F), -10.0F, 89.0F);
        sunIntensity = std::clamp(finiteOr(sunIntensity, 3.8F), 0.0F, 32.0F);
    }

    auto makeSunSkyState(SunSkySettings const& settings) -> SunSkyState
    {
        SunSkySettings sky = settings;
        sky.sanitize();

        const glm::vec3 daySunTint{1.0F, 0.88F, 0.66F};
        const glm::vec3 sunsetTint{1.0F, 0.45F, 0.22F};
        const glm::vec3 dayHorizon{0.54F, 0.68F, 0.86F};
        const glm::vec3 dayZenith{0.08F, 0.18F, 0.38F};
        const glm::vec3 nightHorizon{0.018F, 0.026F, 0.052F};
        const glm::vec3 nightZenith{0.004F, 0.008F, 0.022F};
        const glm::vec3 groundTint{0.015F, 0.018F, 0.026F};
        constexpr float ambientIntensity = 0.72F;
        constexpr float skyIntensity = 1.0F;
        constexpr float sunAngularRadiusDegrees = 0.53F;
        constexpr float sunGlowStrength = 0.18F;

        const float daylight = smoothStep(-4.0F, 10.0F, sky.sunAngleDegrees);
        const float lowSunWarmth =
            smoothStep(-4.0F, 8.0F, sky.sunAngleDegrees) *
            (1.0F - smoothStep(18.0F, 42.0F, sky.sunAngleDegrees));
        const glm::vec3 sunColor = glm::mix(daySunTint * sunsetTint, daySunTint, daylight);
        const glm::vec3 horizonColor =
            glm::mix(nightHorizon, dayHorizon + sunsetTint * lowSunWarmth * 0.28F, daylight);
        const glm::vec3 zenithColor = glm::mix(nightZenith, dayZenith, daylight);
        const glm::vec3 ambientColor =
            (horizonColor * 0.58F + zenithColor * 0.42F) *
            ambientIntensity *
            (0.18F + daylight * 0.82F);

        return SunSkyState{
            .sunDirection = directionFromAngle(sky.sunAngleDegrees),
            .sunIntensity = sky.sunIntensity * daylight,
            .sunColor = sunColor,
            .daylight = daylight,
            .ambientColor = ambientColor,
            .skyIntensity = skyIntensity,
            .skyHorizonColor = horizonColor,
            .sunAngularRadiusRadians = sunAngularRadiusDegrees * std::numbers::pi_v<float> / 180.0F,
            .skyZenithColor = zenithColor,
            .sunGlowStrength = sunGlowStrength * daylight,
            .skyGroundColor = groundTint,
            .padding = 0.0F,
        };
    }

}  // namespace Aerkanis::Environment
