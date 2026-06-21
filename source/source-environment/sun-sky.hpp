#pragma once

#include <glm/glm.hpp>

namespace Aerkanis::Environment
{

    struct SunSkySettings
    {
        bool enabled{true};
        float sunAzimuthDegrees{-122.0F};
        float sunElevationDegrees{38.0F};
        float sunIntensity{3.8F};
        float ambientIntensity{0.72F};
        float skyIntensity{1.0F};
        float sunAngularRadiusDegrees{0.53F};
        float sunGlowStrength{0.18F};
        glm::vec3 sunTint{1.0F, 0.88F, 0.66F};
        glm::vec3 horizonTint{0.54F, 0.68F, 0.86F};
        glm::vec3 zenithTint{0.08F, 0.18F, 0.38F};
        glm::vec3 groundTint{0.015F, 0.018F, 0.026F};

        auto sanitize() -> void;
    };

    struct SunSkyState
    {
        glm::vec3 sunDirection{-0.42F, 0.62F, -0.66F};
        float sunIntensity{3.8F};
        glm::vec3 sunColor{1.0F, 0.88F, 0.66F};
        float daylight{1.0F};
        glm::vec3 ambientColor{0.38F, 0.48F, 0.68F};
        float skyIntensity{1.0F};
        glm::vec3 skyHorizonColor{0.54F, 0.68F, 0.86F};
        float sunAngularRadiusRadians{0.00925F};
        glm::vec3 skyZenithColor{0.08F, 0.18F, 0.38F};
        float sunGlowStrength{0.18F};
        glm::vec3 skyGroundColor{0.015F, 0.018F, 0.026F};
        float padding{};
    };

    auto makeSunSkyState(SunSkySettings const& settings) -> SunSkyState;

}  // namespace Aerkanis::Environment
