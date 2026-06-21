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
#include "vk/buffer.hpp"
#include "vk/descriptor-pool.hpp"
#include "vk/descriptor-set-layout.hpp"
#include "vk/image.hpp"
#include "vk/pipeline-builder.hpp"
#include "vk/shader.hpp"

namespace Aerkanis::Cloud
{

    struct CloudNubisPass
    {
        Context* context{};
        vk::Extent2D renderExtent{};
        CloudSettings settings{};
        CloudTextureUploader uploader{};
        TextureResource weatherTexture{};
        TextureResource curlTexture{};
        TextureResource shapeNoiseTexture{};
        TextureResource detailNoiseTexture{};
        TextureResource lowResolutionModelTexture{};
        TextureResource highResolutionModelTexture{};
        std::vector<Vk::Buffer> parameterBuffers{};
        std::vector<Vk::Image> outputImages{};
        vk::raii::Sampler outputSampler{nullptr};
        std::vector<vk::ImageLayout> outputLayouts{};
        Vk::DescriptorSetLayout computeSetLayout{};
        Vk::DescriptorSetLayout compositeSetLayout{};
        Vk::DescriptorPool descriptorPool{};
        std::vector<vk::raii::DescriptorSet> computeDescriptorSets{};
        std::vector<vk::raii::DescriptorSet> compositeDescriptorSets{};
        Vk::ShaderModule computeShader{};
        Vk::ShaderModule compositeShader{};
        Vk::PipelineBuildResult computePipeline{};
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
            vk::Image targetImage,
            vk::ImageLayout targetLayout,
            Scene::Camera const& camera,
            std::size_t frameIndex) -> void;
        auto drawGui() -> void;

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
