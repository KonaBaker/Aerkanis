#include "source-cloud/cloud-nubis-pass.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "vk/descriptor-writer.hpp"

#if defined(AERKANIS_IMGUI)
#include "imgui.h"
#endif

namespace Aerkanis::Cloud
{

    namespace
    {

        auto outputRange() noexcept -> vk::ImageSubresourceRange
        {
            return vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            };
        }

        auto transitionImage(
            vk::raii::CommandBuffer const& commandBuffer,
            vk::Image image,
            vk::ImageLayout oldLayout,
            vk::ImageLayout newLayout,
            vk::AccessFlags sourceAccess,
            vk::AccessFlags destinationAccess,
            vk::PipelineStageFlags sourceStage,
            vk::PipelineStageFlags destinationStage) -> void
        {
            const vk::ImageMemoryBarrier barrier{
                .srcAccessMask = sourceAccess,
                .dstAccessMask = destinationAccess,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = outputRange(),
            };

            commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
        }

        auto makeLinearClampSampler(vk::raii::Device const& device) -> vk::raii::Sampler
        {
            vk::SamplerCreateInfo samplerCreateInfo{
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = vk::SamplerAddressMode::eClampToEdge,
                .addressModeV = vk::SamplerAddressMode::eClampToEdge,
                .addressModeW = vk::SamplerAddressMode::eClampToEdge,
                .anisotropyEnable = VK_FALSE,
                .maxAnisotropy = 1.0F,
                .compareEnable = VK_FALSE,
                .minLod = 0.0F,
                .maxLod = 0.0F,
                .borderColor = vk::BorderColor::eFloatOpaqueBlack,
                .unnormalizedCoordinates = VK_FALSE,
            };
            return vk::raii::Sampler(device, samplerCreateInfo);
        }

        auto descriptorSetHandle(vk::raii::DescriptorSet const& descriptorSet) noexcept -> vk::DescriptorSet
        {
            return static_cast<vk::DescriptorSet>(*descriptorSet);
        }

    }  // namespace

    auto CloudNubisPass::init(
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
            settings.sanitize();
            uploader.init(nextContext);
            loadResources();
            createParameterBuffers(frameCount);
            createOutputs(extent);
            createDescriptors();
            createPipelines(swapchainFormat);
            updateDescriptors();
            startTime = std::chrono::steady_clock::now();
            initialized = true;
            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[Cloud] Nubis pass init failed: " << exception.what() << '\n';
            shutdown();
            return false;
        }
    }

    auto CloudNubisPass::shutdown() noexcept -> void
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

        computePipeline.pipeline.clear();
        computePipeline.layout.clear();
        compositePipeline.pipeline.clear();
        compositePipeline.layout.clear();
        computeShader.shutdown();
        compositeShader.shutdown();
        computeDescriptorSets.clear();
        compositeDescriptorSets.clear();
        descriptorPool.shutdown();
        computeSetLayout.shutdown();
        compositeSetLayout.shutdown();
        outputSampler.clear();
        for (Vk::Image& outputImage : outputImages)
        {
            outputImage.shutdown();
        }
        outputImages.clear();
        for (Vk::Buffer& parameterBuffer : parameterBuffers)
        {
            parameterBuffer.shutdown();
        }
        parameterBuffers.clear();
        highResolutionModelTexture.shutdown();
        lowResolutionModelTexture.shutdown();
        detailNoiseTexture.shutdown();
        shapeNoiseTexture.shutdown();
        curlTexture.shutdown();
        weatherTexture.shutdown();
        uploader.shutdown();
        outputLayouts.clear();
        renderExtent = {};
        context = nullptr;
        initialized = false;
        resourcesLoaded = false;
    }

    auto CloudNubisPass::recreate(vk::Format swapchainFormat, vk::Extent2D extent) -> bool
    {
        if (context == nullptr)
        {
            return false;
        }

        try
        {
            context->device.waitIdle();
            computePipeline.pipeline.clear();
            computePipeline.layout.clear();
            compositePipeline.pipeline.clear();
            compositePipeline.layout.clear();
            computeDescriptorSets.clear();
            compositeDescriptorSets.clear();
            descriptorPool.shutdown();
            outputSampler.clear();
            for (Vk::Image& outputImage : outputImages)
            {
                outputImage.shutdown();
            }
            outputImages.clear();
            outputLayouts.clear();
            renderExtent = extent;
            createOutputs(extent);
            createDescriptors();
            createPipelines(swapchainFormat);
            updateDescriptors();
            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[Cloud] Nubis pass recreate failed: " << exception.what() << '\n';
            initialized = false;
            return false;
        }
    }

    auto CloudNubisPass::record(
        vk::raii::CommandBuffer const& commandBuffer,
        vk::ImageView targetView,
        vk::Image targetImage,
        vk::ImageLayout targetLayout,
        Scene::Camera const& camera,
        std::size_t frameIndex) -> void
    {
        if (!initialized ||
            !settings.enabled ||
            renderExtent.width == 0 ||
            renderExtent.height == 0 ||
            computeDescriptorSets.empty() ||
            compositeDescriptorSets.empty())
        {
            return;
        }
        (void)targetImage;

        const std::size_t safeFrameIndex = frameIndex % computeDescriptorSets.size();
        if (descriptorSetHandle(computeDescriptorSets[safeFrameIndex]) == VK_NULL_HANDLE ||
            safeFrameIndex >= parameterBuffers.size() ||
            safeFrameIndex >= outputImages.size() ||
            safeFrameIndex >= outputLayouts.size() ||
            safeFrameIndex >= compositeDescriptorSets.size() ||
            descriptorSetHandle(compositeDescriptorSets[safeFrameIndex]) == VK_NULL_HANDLE)
        {
            return;
        }

        settings.sanitize();

        const CloudNubisParameters parameters =
            makeCloudNubisParameters(settings, camera, renderExtent, elapsedSeconds());
        parameterBuffers[safeFrameIndex].writeObject(parameters);

        transitionImage(
            commandBuffer,
            outputImages[safeFrameIndex].handle(),
            outputLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::ImageLayout::eUndefined
                : vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eGeneral,
            outputLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::AccessFlags{}
                : vk::AccessFlagBits::eShaderRead,
            vk::AccessFlagBits::eShaderWrite,
            outputLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::PipelineStageFlagBits::eTopOfPipe
                : vk::PipelineStageFlagBits::eFragmentShader,
            vk::PipelineStageFlagBits::eComputeShader);
        outputLayouts[safeFrameIndex] = vk::ImageLayout::eGeneral;

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::Pipeline>(*computePipeline.pipeline));
        const vk::DescriptorSet computeSet = descriptorSetHandle(computeDescriptorSets[safeFrameIndex]);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::PipelineLayout>(*computePipeline.layout),
            0,
            computeSet,
            nullptr);

        const uint32_t groupCountX = (renderExtent.width + 7U) / 8U;
        const uint32_t groupCountY = (renderExtent.height + 7U) / 8U;
        commandBuffer.dispatch(groupCountX, groupCountY, 1);

        transitionImage(
            commandBuffer,
            outputImages[safeFrameIndex].handle(),
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eShaderWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eFragmentShader);
        outputLayouts[safeFrameIndex] = vk::ImageLayout::eShaderReadOnlyOptimal;

        const vk::RenderingAttachmentInfo colorAttachment{
            .imageView = targetView,
            .imageLayout = targetLayout,
            .loadOp = vk::AttachmentLoadOp::eLoad,
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
            static_cast<vk::Pipeline>(*compositePipeline.pipeline));

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

        const vk::DescriptorSet compositeSet = descriptorSetHandle(compositeDescriptorSets[safeFrameIndex]);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            static_cast<vk::PipelineLayout>(*compositePipeline.layout),
            0,
            compositeSet,
            nullptr);
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRendering();
    }

    auto CloudNubisPass::drawGui() -> void
    {
#if defined(AERKANIS_IMGUI)
        if (ImGui::CollapsingHeader("Cloud Nubis", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enabled", &settings.enabled);
            ImGui::SliderInt("Ray Steps", &settings.stepCount, 8, 160);
            ImGui::SliderInt("Shadow Steps", &settings.shadowStepCount, 0, 16);
            ImGui::DragFloat("Layer Bottom", &settings.layerBottom, 0.01F, -8.0F, 8.0F, "%.2f");
            ImGui::DragFloat("Layer Top", &settings.layerTop, 0.01F, -8.0F, 8.0F, "%.2f");
            ImGui::SliderFloat("Coverage Bias", &settings.coverageOffset, -1.0F, 1.0F, "%.2f");
            ImGui::SliderFloat("Coverage", &settings.coverageMultiplier, 0.0F, 4.0F, "%.2f");
            ImGui::SliderFloat("Density", &settings.densityMultiplier, 0.0F, 4.0F, "%.2f");
            ImGui::SliderFloat("Extinction", &settings.extinction, 0.01F, 4.0F, "%.2f");
            ImGui::SliderFloat("Detail Strength", &settings.detailStrength, 0.0F, 1.0F, "%.2f");
            ImGui::SliderFloat("Type Blend", &settings.typeBlend, 0.0F, 1.0F, "%.2f");
            ImGui::SliderFloat("Phase G", &settings.phaseG, -0.9F, 0.9F, "%.2f");
            ImGui::SliderFloat("Powder", &settings.powderStrength, 0.0F, 2.0F, "%.2f");
            ImGui::DragFloat("Wind Speed", &settings.windSpeed, 0.001F, -1.0F, 1.0F, "%.3f");
            ImGui::DragFloat2("Wind Direction", &settings.windDirection.x, 0.01F, -1.0F, 1.0F, "%.2f");
            ImGui::DragFloat3("Sun Direction", &settings.sunDirection.x, 0.01F, -1.0F, 1.0F, "%.2f");
            ImGui::ColorEdit3("Sun Color", &settings.sunColor.x);
            ImGui::ColorEdit3("Ambient", &settings.ambientColor.x);
        }
#endif

        settings.sanitize();
    }

    auto CloudNubisPass::loadResources() -> void
    {
        if (resourcesLoaded)
        {
            return;
        }

        const std::filesystem::path cloudData = findCloudAssetPath("cloud-data");
        weatherTexture = uploader.uploadTexture2D(cloudData / "noise" / "weather.png");
        curlTexture = uploader.uploadTexture2D(cloudData / "noise" / "curlNoise.png");
        shapeNoiseTexture = uploader.uploadVolumeSlices(
            cloudData / "noise",
            "NubisVoxelCloudNoise(",
            ").tga",
            128);
        detailNoiseTexture = uploader.uploadVolumeSlices(
            cloudData / "modeling-data" / "high-resulotion",
            "hiResClouds (",
            ").tga",
            32);
        lowResolutionModelTexture = uploader.uploadVolumeSlices(
            cloudData / "modeling-data" / "low-resulotion",
            "lowResCloud(",
            ").tga",
            128);
        highResolutionModelTexture = uploader.uploadVolumeSlices(
            cloudData / "modeling-data" / "high-resulotion",
            "hiResClouds (",
            ").tga",
            32);
        resourcesLoaded = true;
    }

    auto CloudNubisPass::createParameterBuffers(std::size_t frameCount) -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Cloud parameter buffer requires a context");
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
                    .size = static_cast<vk::DeviceSize>(sizeof(CloudNubisParameters)),
                    .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                    .memoryProperties =
                        vk::MemoryPropertyFlagBits::eHostVisible |
                        vk::MemoryPropertyFlagBits::eHostCoherent,
                });
        }
    }

    auto CloudNubisPass::createOutputs(vk::Extent2D extent) -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Cloud output requires a context");
        }
        if (extent.width == 0 || extent.height == 0)
        {
            throw std::runtime_error("Cloud output requires a non-zero extent");
        }

        outputImages.clear();
        outputImages.resize(std::max<std::size_t>(parameterBuffers.size(), 1));
        outputLayouts.assign(outputImages.size(), vk::ImageLayout::eUndefined);

        for (Vk::Image& outputImage : outputImages)
        {
            outputImage.init(
                *context,
                Vk::ImageConfig{
                    .imageType = vk::ImageType::e2D,
                    .format = vk::Format::eR16G16B16A16Sfloat,
                    .extent = vk::Extent3D{
                        .width = extent.width,
                        .height = extent.height,
                        .depth = 1,
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage =
                        vk::ImageUsageFlagBits::eStorage |
                        vk::ImageUsageFlagBits::eSampled,
                    .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                    .createView = true,
                    .view = Vk::ImageViewConfig{
                        .format = vk::Format::eR16G16B16A16Sfloat,
                        .viewType = vk::ImageViewType::e2D,
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                });
        }
        outputSampler = makeLinearClampSampler(context->device);
    }

    auto CloudNubisPass::createDescriptors() -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Cloud descriptors require a context");
        }

        computeSetLayout.shutdown();
        compositeSetLayout.shutdown();

        computeSetLayout
            .addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute)
            .addBinding(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(2, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(3, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(4, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(5, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(6, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(7, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(8, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eCompute)
            .build(context->device);

        compositeSetLayout
            .addBinding(0, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .addBinding(1, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment)
            .build(context->device);

        const uint32_t computeSetCount = static_cast<uint32_t>(std::max<std::size_t>(parameterBuffers.size(), 1));
        const std::array poolSizes{
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eStorageImage,
                .descriptorCount = computeSetCount,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = computeSetCount,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampledImage,
                .descriptorCount = computeSetCount * 7U,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampler,
                .descriptorCount = computeSetCount * 2U,
            },
        };

        descriptorPool.init(
            context->device,
            computeSetCount * 2U,
            poolSizes);
        computeDescriptorSets.clear();
        computeDescriptorSets.reserve(parameterBuffers.size());
        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            computeDescriptorSets.emplace_back(descriptorPool.allocate(context->device, computeSetLayout));
        }
        compositeDescriptorSets.clear();
        compositeDescriptorSets.reserve(parameterBuffers.size());
        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            compositeDescriptorSets.emplace_back(descriptorPool.allocate(context->device, compositeSetLayout));
        }
    }

    auto CloudNubisPass::updateDescriptors() -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Cloud descriptor update requires a context");
        }
        if (outputImages.size() < computeDescriptorSets.size() ||
            outputImages.size() < compositeDescriptorSets.size())
        {
            throw std::runtime_error("Cloud descriptor update requires per-frame output images");
        }

        Vk::DescriptorWriter writer{};
        for (std::size_t index = 0; index < computeDescriptorSets.size(); ++index)
        {
            writer.writeBuffer(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    0,
                    vk::DescriptorType::eUniformBuffer,
                    parameterBuffers[index].descriptorInfo())
                .writeImage(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    1,
                    vk::DescriptorType::eStorageImage,
                    vk::DescriptorImageInfo{
                        .imageView = outputImages[index].viewHandle(),
                        .imageLayout = vk::ImageLayout::eGeneral,
                    })
                .writeImage(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    2,
                    vk::DescriptorType::eSampledImage,
                    weatherTexture.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    3,
                    vk::DescriptorType::eSampledImage,
                    curlTexture.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    4,
                    vk::DescriptorType::eSampledImage,
                    shapeNoiseTexture.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    5,
                    vk::DescriptorType::eSampledImage,
                    detailNoiseTexture.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    6,
                    vk::DescriptorType::eSampledImage,
                    lowResolutionModelTexture.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    7,
                    vk::DescriptorType::eSampledImage,
                    highResolutionModelTexture.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(computeDescriptorSets[index]),
                    8,
                    vk::DescriptorType::eSampler,
                    vk::DescriptorImageInfo{
                        .sampler = static_cast<vk::Sampler>(*weatherTexture.sampler),
                    });
        }

        for (std::size_t index = 0; index < compositeDescriptorSets.size(); ++index)
        {
            writer.writeImage(
                    descriptorSetHandle(compositeDescriptorSets[index]),
                    0,
                    vk::DescriptorType::eSampledImage,
                    vk::DescriptorImageInfo{
                        .imageView = outputImages[index].viewHandle(),
                        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    })
                .writeImage(
                    descriptorSetHandle(compositeDescriptorSets[index]),
                    1,
                    vk::DescriptorType::eSampler,
                    vk::DescriptorImageInfo{
                        .sampler = static_cast<vk::Sampler>(*outputSampler),
                    });
        }
        writer.update(context->device);
    }

    auto CloudNubisPass::createPipelines(vk::Format swapchainFormat) -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Cloud pipelines require a context");
        }

        computeShader.shutdown();
        compositeShader.shutdown();
        computeShader.init(context->device, shaderPath("cloud/cloud-nubis.spv"));
        compositeShader.init(context->device, shaderPath("cloud/cloud-composite.spv"));

        Vk::PipelineBuilder computeBuilder{};
        computeBuilder.addShaderStage(computeShader, vk::ShaderStageFlagBits::eCompute, "compMain")
            .addDescriptorSetLayout(computeSetLayout);
        computePipeline = computeBuilder.buildCompute(context->device);

        Vk::PipelineBuilder compositeBuilder{};
        compositeBuilder.addShaderStage(compositeShader, vk::ShaderStageFlagBits::eVertex, "vertMain")
            .addShaderStage(compositeShader, vk::ShaderStageFlagBits::eFragment, "fragMain")
            .addDescriptorSetLayout(compositeSetLayout)
            .setColorAttachmentFormat(swapchainFormat)
            .setCullMode(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
            .disableDepth();
        compositePipeline = compositeBuilder.buildGraphics(context->device);
    }

    auto CloudNubisPass::shaderPath(std::filesystem::path const& relativePath) const
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

    auto CloudNubisPass::elapsedSeconds() const -> float
    {
        if (startTime.time_since_epoch().count() == 0)
        {
            return 0.0F;
        }

        return std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
    }

}  // namespace Aerkanis::Cloud
