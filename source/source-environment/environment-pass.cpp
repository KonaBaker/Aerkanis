#include "source-environment/environment-pass.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include <glm/gtc/matrix_inverse.hpp>

#include "vk/descriptor-writer.hpp"

#if defined(AERKANIS_IMGUI)
#include "imgui.h"
#endif

namespace Aerkanis::Environment
{

    namespace
    {

        struct alignas(16) SunSkyParameters
        {
            glm::vec4 inverseViewProjectionRow0{};
            glm::vec4 inverseViewProjectionRow1{};
            glm::vec4 inverseViewProjectionRow2{};
            glm::vec4 inverseViewProjectionRow3{};
            glm::vec4 cameraPositionExposure{};
            glm::vec4 sunDirectionIntensity{};
            glm::vec4 sunColorDaylight{};
            glm::vec4 ambientColorSkyIntensity{};
            glm::vec4 skyHorizonSunRadius{};
            glm::vec4 skyZenithSunGlow{};
            glm::vec4 skyGroundColor{};
        };

        static_assert(sizeof(SunSkyParameters) % 16 == 0);

        auto descriptorSetHandle(vk::raii::DescriptorSet const& descriptorSet) noexcept -> vk::DescriptorSet
        {
            return static_cast<vk::DescriptorSet>(*descriptorSet);
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

        auto makeSunSkyParameters(
            SunSkyState const& sunSky,
            Scene::Camera const& camera,
            vk::Extent2D extent) -> SunSkyParameters
        {
            Scene::Camera sanitizedCamera = camera;
            sanitizedCamera.sanitize();

            const float aspectRatio =
                extent.height == 0
                    ? 1.0F
                    : static_cast<float>(extent.width) / static_cast<float>(extent.height);
            const glm::mat4 inverseViewProjection = glm::inverse(sanitizedCamera.viewProjection(aspectRatio));

            return SunSkyParameters{
                .inverseViewProjectionRow0 = matrixRow(inverseViewProjection, 0),
                .inverseViewProjectionRow1 = matrixRow(inverseViewProjection, 1),
                .inverseViewProjectionRow2 = matrixRow(inverseViewProjection, 2),
                .inverseViewProjectionRow3 = matrixRow(inverseViewProjection, 3),
                .cameraPositionExposure = glm::vec4{sanitizedCamera.position, 1.0F},
                .sunDirectionIntensity = glm::vec4{glm::normalize(sunSky.sunDirection), sunSky.sunIntensity},
                .sunColorDaylight = glm::vec4{sunSky.sunColor, sunSky.daylight},
                .ambientColorSkyIntensity = glm::vec4{sunSky.ambientColor, sunSky.skyIntensity},
                .skyHorizonSunRadius = glm::vec4{sunSky.skyHorizonColor, sunSky.sunAngularRadiusRadians},
                .skyZenithSunGlow = glm::vec4{sunSky.skyZenithColor, sunSky.sunGlowStrength},
                .skyGroundColor = glm::vec4{sunSky.skyGroundColor, 1.0F},
            };
        }

    }  // namespace

    auto EnvironmentPass::init(
        Context& nextContext,
        vk::Format swapchainFormat,
        vk::Extent2D extent,
        std::size_t frameCount) -> bool
    {
        shutdown();

        try
        {
            context = &nextContext;
            renderExtent = extent;
            createParameterBuffers(frameCount);
            createDescriptors();
            createPipeline(swapchainFormat);
            updateDescriptors();
            initialized = true;
            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[Environment] pass init failed: " << exception.what() << '\n';
            shutdown();
            return false;
        }
    }

    auto EnvironmentPass::shutdown() noexcept -> void
    {
        try
        {
            if (context != nullptr && static_cast<vk::Device>(*context->device) != VK_NULL_HANDLE)
            {
                context->device.waitIdle();
            }
        }
        catch (...)
        {
        }

        pipeline.pipeline.clear();
        pipeline.layout.clear();
        shader.shutdown();
        descriptorSets.clear();
        descriptorPool.shutdown();
        descriptorSetLayout.shutdown();
        for (Vk::Buffer& parameterBuffer : parameterBuffers)
        {
            parameterBuffer.shutdown();
        }
        parameterBuffers.clear();
        renderExtent = {};
        context = nullptr;
        initialized = false;
    }

    auto EnvironmentPass::recreate(vk::Format swapchainFormat, vk::Extent2D extent) -> bool
    {
        if (context == nullptr)
        {
            return false;
        }

        try
        {
            context->device.waitIdle();
            pipeline.pipeline.clear();
            pipeline.layout.clear();
            shader.shutdown();
            renderExtent = extent;
            createPipeline(swapchainFormat);
            initialized = true;
            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[Environment] pass recreate failed: " << exception.what() << '\n';
            initialized = false;
            return false;
        }
    }

    auto EnvironmentPass::record(
        vk::raii::CommandBuffer const& commandBuffer,
        vk::ImageView targetView,
        vk::ImageLayout targetLayout,
        SunSkyState const& sunSky,
        Scene::Camera const& camera,
        std::size_t frameIndex) -> void
    {
        if (!initialized ||
            renderExtent.width == 0 ||
            renderExtent.height == 0 ||
            descriptorSets.empty())
        {
            return;
        }

        const std::size_t safeFrameIndex = frameIndex % descriptorSets.size();
        if (descriptorSetHandle(descriptorSets[safeFrameIndex]) == VK_NULL_HANDLE ||
            safeFrameIndex >= parameterBuffers.size())
        {
            return;
        }

        const SunSkyParameters parameters = makeSunSkyParameters(sunSky, camera, renderExtent);
        parameterBuffers[safeFrameIndex].writeObject(parameters);

        const vk::RenderingAttachmentInfo colorAttachment{
            .imageView = targetView,
            .imageLayout = targetLayout,
            .loadOp = vk::AttachmentLoadOp::eDontCare,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        const vk::RenderingInfo renderingInfo{
            .renderArea = vk::Rect2D{
                .offset = vk::Offset2D{.x = 0, .y = 0},
                .extent = renderExtent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
        };

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            static_cast<vk::Pipeline>(*pipeline.pipeline));

        const vk::Viewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(renderExtent.width),
            .height = static_cast<float>(renderExtent.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F,
        };
        const vk::Rect2D scissor{
            .offset = vk::Offset2D{.x = 0, .y = 0},
            .extent = renderExtent,
        };
        commandBuffer.setViewport(0, viewport);
        commandBuffer.setScissor(0, scissor);

        const vk::DescriptorSet descriptorSet = descriptorSetHandle(descriptorSets[safeFrameIndex]);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            static_cast<vk::PipelineLayout>(*pipeline.layout),
            0,
            descriptorSet,
            nullptr);
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRendering();
    }

    auto EnvironmentPass::drawGui(SunSkySettings& settings) -> void
    {
#if defined(AERKANIS_IMGUI)
        if (ImGui::CollapsingHeader("Sun Sky", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Sun Angle", &settings.sunAngleDegrees, -10.0F, 89.0F, "%.1f");
            ImGui::SliderFloat("Sun Intensity", &settings.sunIntensity, 0.0F, 16.0F, "%.2f");
        }
#endif

        settings.sanitize();
    }

    auto EnvironmentPass::createParameterBuffers(std::size_t frameCount) -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Environment parameter buffer requires a context");
        }

        for (Vk::Buffer& parameterBuffer : parameterBuffers)
        {
            parameterBuffer.shutdown();
        }
        parameterBuffers.clear();
        parameterBuffers.resize(std::max<std::size_t>(frameCount, 1));

        for (Vk::Buffer& parameterBuffer : parameterBuffers)
        {
            parameterBuffer.init(
                *context,
                Vk::BufferConfig{
                    .size = static_cast<vk::DeviceSize>(sizeof(SunSkyParameters)),
                    .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                    .memoryProperties =
                        vk::MemoryPropertyFlagBits::eHostVisible |
                        vk::MemoryPropertyFlagBits::eHostCoherent,
                });
        }
    }

    auto EnvironmentPass::createDescriptors() -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Environment descriptors require a context");
        }

        descriptorSetLayout.shutdown();
        descriptorSetLayout
            .addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eFragment)
            .build(context->device);

        const uint32_t setCount = static_cast<uint32_t>(std::max<std::size_t>(parameterBuffers.size(), 1));
        const std::array poolSizes{
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = setCount,
            },
        };

        descriptorPool.init(context->device, setCount, poolSizes);
        descriptorSets.clear();
        descriptorSets.reserve(parameterBuffers.size());
        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            descriptorSets.emplace_back(descriptorPool.allocate(context->device, descriptorSetLayout));
        }
    }

    auto EnvironmentPass::updateDescriptors() -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Environment descriptor update requires a context");
        }

        Vk::DescriptorWriter writer{};
        for (std::size_t index = 0; index < descriptorSets.size(); ++index)
        {
            writer.writeBuffer(
                descriptorSetHandle(descriptorSets[index]),
                0,
                vk::DescriptorType::eUniformBuffer,
                parameterBuffers[index].descriptorInfo());
        }
        writer.update(context->device);
    }

    auto EnvironmentPass::createPipeline(vk::Format swapchainFormat) -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Environment pipeline requires a context");
        }

        shader.shutdown();
        shader.init(context->device, shaderPath("environment/sun-sky.spv"));

        Vk::PipelineBuilder builder{};
        builder.addShaderStage(shader, vk::ShaderStageFlagBits::eVertex, "vertMain")
            .addShaderStage(shader, vk::ShaderStageFlagBits::eFragment, "fragMain")
            .addDescriptorSetLayout(descriptorSetLayout)
            .setColorAttachmentFormat(swapchainFormat)
            .setCullMode(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
            .disableDepth();
        pipeline = builder.buildGraphics(context->device);
    }

    auto EnvironmentPass::shaderPath(std::filesystem::path const& relativePath) const
        -> std::filesystem::path
    {
        const std::array candidates{
            std::filesystem::current_path() / "shaders" / relativePath,
            std::filesystem::current_path() / ".." / "shaders" / relativePath,
            std::filesystem::current_path() / "build" / "shaders" / relativePath,
        };

        for (std::filesystem::path const& candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                return candidate;
            }
        }

        return candidates.front();
    }

}  // namespace Aerkanis::Environment
