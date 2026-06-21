#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"
#include "scene/camera.hpp"
#include "source-environment/sun-sky.hpp"
#include "vk/buffer.hpp"
#include "vk/descriptor-pool.hpp"
#include "vk/descriptor-set-layout.hpp"
#include "vk/pipeline-builder.hpp"
#include "vk/shader.hpp"

namespace Aerkanis::Environment
{

    struct EnvironmentPass
    {
        Context* context{};
        vk::Extent2D renderExtent{};
        std::vector<Vk::Buffer> parameterBuffers{};
        Vk::DescriptorSetLayout descriptorSetLayout{};
        Vk::DescriptorPool descriptorPool{};
        std::vector<vk::raii::DescriptorSet> descriptorSets{};
        Vk::ShaderModule shader{};
        Vk::PipelineBuildResult pipeline{};
        bool initialized{false};

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
            SunSkyState const& sunSky,
            Scene::Camera const& camera,
            std::size_t frameIndex) -> void;
        auto drawGui(SunSkySettings& settings) -> void;

    private:
        auto createParameterBuffers(std::size_t frameCount) -> void;
        auto createDescriptors() -> void;
        auto updateDescriptors() -> void;
        auto createPipeline(vk::Format swapchainFormat) -> void;
        auto shaderPath(std::filesystem::path const& relativePath) const -> std::filesystem::path;
    };

}  // namespace Aerkanis::Environment
