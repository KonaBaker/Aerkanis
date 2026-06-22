#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"
#include "scene/camera.hpp"
#include "source-cloud/cloud-model.hpp"
#include "source-cloud/cloud-texture.hpp"
#include "source-environment/sun-sky.hpp"
#include "vk/buffer.hpp"
#include "vk/descriptor-pool.hpp"
#include "vk/descriptor-set-layout.hpp"
#include "vk/image.hpp"
#include "vk/pipeline-builder.hpp"
#include "vk/shader.hpp"

namespace Aerkanis::Cloud
{

    struct CloudNubisCubedPass
    {
        Context* context{};
        vk::Extent2D renderExtent{};
        CloudTextureUploader uploader{};
        TextureResource modelingTexture0{};
        TextureResource modelingTexture1{};
        TextureResource detailNoiseTexture{};
        std::vector<Vk::Buffer> parameterBuffers{};
        Vk::Image lightGridImage{};
        std::vector<Vk::Image> nearStateImages{};
        std::vector<Vk::Image> rawOutputImages{};
        std::vector<Vk::Image> outputImages{};
        std::vector<Vk::Image> historyImages{};
        vk::raii::Sampler outputSampler{nullptr};
        vk::ImageLayout lightGridLayout{vk::ImageLayout::eUndefined};
        std::vector<vk::ImageLayout> nearStateLayouts{};
        std::vector<vk::ImageLayout> rawOutputLayouts{};
        std::vector<vk::ImageLayout> outputLayouts{};
        std::vector<vk::ImageLayout> historyLayouts{};
        std::vector<glm::mat4> historyViewProjections{};
        std::vector<bool> historyValid{};
        Vk::DescriptorSetLayout lightGridSetLayout{};
        Vk::DescriptorSetLayout nearSetLayout{};
        Vk::DescriptorSetLayout farSetLayout{};
        Vk::DescriptorSetLayout reprojectSetLayout{};
        Vk::DescriptorSetLayout compositeSetLayout{};
        Vk::DescriptorPool descriptorPool{};
        std::vector<vk::raii::DescriptorSet> lightGridDescriptorSets{};
        std::vector<vk::raii::DescriptorSet> nearDescriptorSets{};
        std::vector<vk::raii::DescriptorSet> farDescriptorSets{};
        std::vector<vk::raii::DescriptorSet> reprojectDescriptorSets{};
        std::vector<vk::raii::DescriptorSet> compositeDescriptorSets{};
        Vk::ShaderModule lightGridShader{};
        Vk::ShaderModule nearShader{};
        Vk::ShaderModule farShader{};
        Vk::ShaderModule reprojectShader{};
        Vk::ShaderModule compositeShader{};
        Vk::PipelineBuildResult lightGridPipeline{};
        Vk::PipelineBuildResult nearPipeline{};
        Vk::PipelineBuildResult farPipeline{};
        Vk::PipelineBuildResult reprojectPipeline{};
        Vk::PipelineBuildResult compositePipeline{};
        std::chrono::steady_clock::time_point startTime{};
        bool initialized{false};
        bool resourcesLoaded{false};

        auto init(
            Context& nextContext,
            vk::Format swapchainFormat,
            vk::Extent2D extent,
            std::size_t frameCount) -> bool;
        auto shutdown() noexcept -> void;
        auto recreate(vk::Format swapchainFormat, vk::Extent2D extent) -> bool;
        auto record(
            vk::raii::CommandBuffer const& commandBuffer,
            vk::ImageView targetView,
            vk::ImageLayout targetLayout,
            Environment::SunSkyState const& sunSky,
            Scene::Camera const& camera,
            CloudNubisCubedSettings const& settings,
            std::size_t frameIndex) -> void;
        auto drawGui(CloudNubisCubedSettings& settings) -> void;

    private:
        auto loadResources() -> void;
        auto createParameterBuffers(std::size_t frameCount) -> void;
        auto createOutputs(vk::Extent2D extent) -> void;
        auto createDescriptors() -> void;
        auto updateDescriptors() -> void;
        auto createPipelines(vk::Format swapchainFormat) -> void;
        auto shaderPath(std::filesystem::path const& relativePath) const -> std::filesystem::path;
        auto elapsedSeconds() const -> float;
    };

}  // namespace Aerkanis::Cloud
