#pragma once

#include <cstdint>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "scene/camera.hpp"
#include "source-environment/sun-sky.hpp"

namespace Aerkanis::Cloud
{

    struct CloudSettings
    {
        bool enabled{true};
        int stepCount{72};
        int shadowStepCount{6};
        float layerBottom{-0.35F};
        float layerTop{1.85F};
        float coverageOffset{-0.08F};
        float coverageMultiplier{1.08F};
        float densityMultiplier{0.72F};
        float extinction{1.0F};
        float absorption{0.72F};
        float weatherScale{0.026F};
        float shapeScale{0.16F};
        float detailScale{0.62F};
        float detailStrength{0.23F};
        float windSpeed{0.028F};
        float typeBlend{0.55F};
        float phaseG{0.48F};
        float powderStrength{0.72F};
        float baseBrightness{0.38F};
        float exposure{1.0F};
        glm::vec2 windDirection{1.0F, 0.28F};

        auto sanitize() -> void;
    };

    struct alignas(16) CloudNubisParameters
    {
        glm::vec4 inverseViewProjectionRow0{};
        glm::vec4 inverseViewProjectionRow1{};
        glm::vec4 inverseViewProjectionRow2{};
        glm::vec4 inverseViewProjectionRow3{};
        glm::vec4 cameraPositionTime{};
        glm::vec4 screenAndSteps{};
        glm::vec4 layerAndCoverage{};
        glm::vec4 windAndScale{};
        glm::vec4 detailAndDensity{};
        glm::vec4 phaseAndPowder{};
        glm::vec4 sunDirectionExposure{};
        glm::vec4 sunColorIntensity{};
        glm::vec4 ambientColorAbsorption{};
        glm::vec4 gradientStratus{};
        glm::vec4 gradientCumulus{};
        glm::vec4 gradientCumulonimbus{};
        glm::vec4 skyHorizonColor{};
        glm::vec4 skyZenithColor{};
    };

    static_assert(sizeof(CloudNubisParameters) % 16 == 0);

    auto makeCloudNubisParameters(
        CloudSettings const& settings,
        Environment::SunSkyState const& sunSky,
        Scene::Camera const& camera,
        vk::Extent2D extent,
        float elapsedSeconds) -> CloudNubisParameters;

}  // namespace Aerkanis::Cloud
