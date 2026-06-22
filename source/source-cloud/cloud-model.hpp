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

    struct CloudNubisCubedSettings
    {
        bool enabled{true};
        bool lightGridEnabled{true};
        bool nearFarSplitEnabled{true};
        bool temporalReprojectionEnabled{true};
        int datasetIndex{0};
        int stepCount{104};
        int shadowStepCount{5};
        float voxelScale{1.0F};
        float nearSplitDistance{3.2F};
        float densityMultiplier{1.15F};
        float detailScale{2.65F};
        float detailStrength{0.42F};
        float farClip{96.0F};
        float transmittanceLimit{0.018F};
        float temporalBlend{0.82F};
        float windSpeed{0.018F};
        float phaseG{0.58F};
        float powderStrength{0.64F};
        float baseBrightness{0.42F};
        float absorption{0.82F};
        float exposure{1.0F};
        glm::vec2 windDirection{1.0F, 0.24F};

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

    struct alignas(16) CloudNubisCubedParameters
    {
        glm::vec4 inverseViewProjectionRow0{};
        glm::vec4 inverseViewProjectionRow1{};
        glm::vec4 inverseViewProjectionRow2{};
        glm::vec4 inverseViewProjectionRow3{};
        glm::vec4 cameraPositionTime{};
        glm::vec4 screenAndSteps{};
        glm::vec4 boundsMinDataset{};
        glm::vec4 boundsMaxDensity{};
        glm::vec4 detailAndMarch{};
        glm::vec4 windAndExposure{};
        glm::vec4 lighting{};
        glm::vec4 sunDirectionAbsorption{};
        glm::vec4 sunColorIntensity{};
        glm::vec4 ambientColor{};
        glm::vec4 skyHorizonColor{};
        glm::vec4 skyZenithColor{};
        glm::vec4 previousViewProjectionRow0{};
        glm::vec4 previousViewProjectionRow1{};
        glm::vec4 previousViewProjectionRow2{};
        glm::vec4 previousViewProjectionRow3{};
        glm::vec4 splitAndGrid{};
        glm::vec4 featureFlags{};
    };

    static_assert(sizeof(CloudNubisCubedParameters) % 16 == 0);

    inline constexpr uint32_t NubisCubedLightGridHorizontalSize = 128U;
    inline constexpr uint32_t NubisCubedLightGridVerticalSize = 32U;

    auto makeCloudNubisParameters(
        CloudSettings const& settings,
        Environment::SunSkyState const& sunSky,
        Scene::Camera const& camera,
        vk::Extent2D extent,
        float elapsedSeconds) -> CloudNubisParameters;

    auto makeCloudNubisCubedParameters(
        CloudNubisCubedSettings const& settings,
        Environment::SunSkyState const& sunSky,
        Scene::Camera const& camera,
        vk::Extent2D extent,
        float elapsedSeconds,
        glm::mat4 const& previousViewProjection,
        bool historyValid) -> CloudNubisCubedParameters;

}  // namespace Aerkanis::Cloud
