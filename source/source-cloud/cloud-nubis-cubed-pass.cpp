#include "source-cloud/cloud-nubis-cubed-pass.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
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

    auto CloudNubisCubedPass::init(
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
            std::cerr << "[Cloud] Nubis3 pass init failed: " << exception.what() << '\n';
            shutdown();
            return false;
        }
    }

    auto CloudNubisCubedPass::shutdown() noexcept -> void
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

        lightGridPipeline.pipeline.clear();
        lightGridPipeline.layout.clear();
        nearPipeline.pipeline.clear();
        nearPipeline.layout.clear();
        farPipeline.pipeline.clear();
        farPipeline.layout.clear();
        reprojectPipeline.pipeline.clear();
        reprojectPipeline.layout.clear();
        compositePipeline.pipeline.clear();
        compositePipeline.layout.clear();
        lightGridShader.shutdown();
        nearShader.shutdown();
        farShader.shutdown();
        reprojectShader.shutdown();
        compositeShader.shutdown();
        lightGridDescriptorSets.clear();
        nearDescriptorSets.clear();
        farDescriptorSets.clear();
        reprojectDescriptorSets.clear();
        compositeDescriptorSets.clear();
        descriptorPool.shutdown();
        lightGridSetLayout.shutdown();
        nearSetLayout.shutdown();
        farSetLayout.shutdown();
        reprojectSetLayout.shutdown();
        compositeSetLayout.shutdown();
        outputSampler.clear();
        lightGridImage.shutdown();
        for (Vk::Image& nearStateImage : nearStateImages)
        {
            nearStateImage.shutdown();
        }
        nearStateImages.clear();
        for (Vk::Image& rawOutputImage : rawOutputImages)
        {
            rawOutputImage.shutdown();
        }
        rawOutputImages.clear();
        for (Vk::Image& outputImage : outputImages)
        {
            outputImage.shutdown();
        }
        outputImages.clear();
        for (Vk::Image& historyImage : historyImages)
        {
            historyImage.shutdown();
        }
        historyImages.clear();
        for (Vk::Buffer& parameterBuffer : parameterBuffers)
        {
            parameterBuffer.shutdown();
        }
        parameterBuffers.clear();
        detailNoiseTexture.shutdown();
        modelingTexture1.shutdown();
        modelingTexture0.shutdown();
        uploader.shutdown();
        lightGridLayout = vk::ImageLayout::eUndefined;
        nearStateLayouts.clear();
        rawOutputLayouts.clear();
        outputLayouts.clear();
        historyLayouts.clear();
        historyViewProjections.clear();
        historyValid.clear();
        renderExtent = {};
        context = nullptr;
        initialized = false;
        resourcesLoaded = false;
    }

    auto CloudNubisCubedPass::recreate(vk::Format swapchainFormat, vk::Extent2D extent) -> bool
    {
        if (context == nullptr)
        {
            return false;
        }

        try
        {
            context->device.waitIdle();
            lightGridPipeline.pipeline.clear();
            lightGridPipeline.layout.clear();
            nearPipeline.pipeline.clear();
            nearPipeline.layout.clear();
            farPipeline.pipeline.clear();
            farPipeline.layout.clear();
            reprojectPipeline.pipeline.clear();
            reprojectPipeline.layout.clear();
            compositePipeline.pipeline.clear();
            compositePipeline.layout.clear();
            lightGridDescriptorSets.clear();
            nearDescriptorSets.clear();
            farDescriptorSets.clear();
            reprojectDescriptorSets.clear();
            compositeDescriptorSets.clear();
            descriptorPool.shutdown();
            outputSampler.clear();
            lightGridImage.shutdown();
            for (Vk::Image& nearStateImage : nearStateImages)
            {
                nearStateImage.shutdown();
            }
            nearStateImages.clear();
            for (Vk::Image& rawOutputImage : rawOutputImages)
            {
                rawOutputImage.shutdown();
            }
            rawOutputImages.clear();
            for (Vk::Image& outputImage : outputImages)
            {
                outputImage.shutdown();
            }
            outputImages.clear();
            for (Vk::Image& historyImage : historyImages)
            {
                historyImage.shutdown();
            }
            historyImages.clear();
            lightGridLayout = vk::ImageLayout::eUndefined;
            nearStateLayouts.clear();
            rawOutputLayouts.clear();
            outputLayouts.clear();
            historyLayouts.clear();
            historyViewProjections.clear();
            historyValid.clear();
            renderExtent = extent;
            createOutputs(extent);
            createDescriptors();
            createPipelines(swapchainFormat);
            updateDescriptors();
            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[Cloud] Nubis3 pass recreate failed: " << exception.what() << '\n';
            initialized = false;
            return false;
        }
    }

    auto CloudNubisCubedPass::record(
        vk::raii::CommandBuffer const& commandBuffer,
        vk::ImageView targetView,
        vk::ImageLayout targetLayout,
        Environment::SunSkyState const& sunSky,
        Scene::Camera const& camera,
        CloudNubisCubedSettings const& settings,
        std::size_t frameIndex) -> void
    {
        CloudNubisCubedSettings cloud = settings;
        cloud.sanitize();

        if (!initialized ||
            !cloud.enabled ||
            renderExtent.width == 0 ||
            renderExtent.height == 0 ||
            lightGridDescriptorSets.empty() ||
            nearDescriptorSets.empty() ||
            farDescriptorSets.empty() ||
            reprojectDescriptorSets.empty() ||
            compositeDescriptorSets.empty())
        {
            return;
        }

        const std::size_t safeFrameIndex = frameIndex % lightGridDescriptorSets.size();
        if (descriptorSetHandle(lightGridDescriptorSets[safeFrameIndex]) == VK_NULL_HANDLE ||
            descriptorSetHandle(nearDescriptorSets[safeFrameIndex]) == VK_NULL_HANDLE ||
            descriptorSetHandle(farDescriptorSets[safeFrameIndex]) == VK_NULL_HANDLE ||
            descriptorSetHandle(reprojectDescriptorSets[safeFrameIndex]) == VK_NULL_HANDLE ||
            safeFrameIndex >= parameterBuffers.size() ||
            safeFrameIndex >= nearStateImages.size() ||
            safeFrameIndex >= rawOutputImages.size() ||
            safeFrameIndex >= outputImages.size() ||
            safeFrameIndex >= historyImages.size() ||
            safeFrameIndex >= nearStateLayouts.size() ||
            safeFrameIndex >= rawOutputLayouts.size() ||
            safeFrameIndex >= outputLayouts.size() ||
            safeFrameIndex >= historyLayouts.size() ||
            safeFrameIndex >= historyViewProjections.size() ||
            safeFrameIndex >= historyValid.size() ||
            safeFrameIndex >= compositeDescriptorSets.size() ||
            descriptorSetHandle(compositeDescriptorSets[safeFrameIndex]) == VK_NULL_HANDLE)
        {
            return;
        }

        Scene::Camera sanitizedCamera = camera;
        sanitizedCamera.sanitize();
        const float aspectRatio =
            renderExtent.height == 0
                ? 1.0F
                : static_cast<float>(renderExtent.width) / static_cast<float>(renderExtent.height);
        const glm::mat4 currentViewProjection = sanitizedCamera.viewProjection(aspectRatio);
        const bool validHistory = cloud.temporalReprojectionEnabled && historyValid[safeFrameIndex];
        const CloudNubisCubedParameters parameters =
            makeCloudNubisCubedParameters(
                cloud,
                sunSky,
                camera,
                renderExtent,
                elapsedSeconds(),
                historyViewProjections[safeFrameIndex],
                validHistory);
        parameterBuffers[safeFrameIndex].writeObject(parameters);

        transitionImage(
            commandBuffer,
            lightGridImage.handle(),
            lightGridLayout == vk::ImageLayout::eUndefined
                ? vk::ImageLayout::eUndefined
                : vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eGeneral,
            lightGridLayout == vk::ImageLayout::eUndefined
                ? vk::AccessFlags{}
                : vk::AccessFlagBits::eShaderRead,
            vk::AccessFlagBits::eShaderWrite,
            lightGridLayout == vk::ImageLayout::eUndefined
                ? vk::PipelineStageFlagBits::eTopOfPipe
                : vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader);
        lightGridLayout = vk::ImageLayout::eGeneral;

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::Pipeline>(*lightGridPipeline.pipeline));
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::PipelineLayout>(*lightGridPipeline.layout),
            0,
            descriptorSetHandle(lightGridDescriptorSets[safeFrameIndex]),
            nullptr);
        commandBuffer.dispatch(
            (NubisCubedLightGridHorizontalSize + 3U) / 4U,
            (NubisCubedLightGridHorizontalSize + 3U) / 4U,
            (NubisCubedLightGridVerticalSize + 3U) / 4U);

        transitionImage(
            commandBuffer,
            lightGridImage.handle(),
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eShaderWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader);
        lightGridLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        transitionImage(
            commandBuffer,
            nearStateImages[safeFrameIndex].handle(),
            nearStateLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::ImageLayout::eUndefined
                : vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eGeneral,
            nearStateLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::AccessFlags{}
                : vk::AccessFlagBits::eShaderRead,
            vk::AccessFlagBits::eShaderWrite,
            nearStateLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::PipelineStageFlagBits::eTopOfPipe
                : vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader);
        nearStateLayouts[safeFrameIndex] = vk::ImageLayout::eGeneral;

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::Pipeline>(*nearPipeline.pipeline));
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::PipelineLayout>(*nearPipeline.layout),
            0,
            descriptorSetHandle(nearDescriptorSets[safeFrameIndex]),
            nullptr);

        const uint32_t groupCountX = (renderExtent.width + 7U) / 8U;
        const uint32_t groupCountY = (renderExtent.height + 7U) / 8U;
        commandBuffer.dispatch(groupCountX, groupCountY, 1);

        transitionImage(
            commandBuffer,
            nearStateImages[safeFrameIndex].handle(),
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eShaderWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader);
        nearStateLayouts[safeFrameIndex] = vk::ImageLayout::eShaderReadOnlyOptimal;

        transitionImage(
            commandBuffer,
            rawOutputImages[safeFrameIndex].handle(),
            rawOutputLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::ImageLayout::eUndefined
                : vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eGeneral,
            rawOutputLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::AccessFlags{}
                : vk::AccessFlagBits::eShaderRead,
            vk::AccessFlagBits::eShaderWrite,
            rawOutputLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined
                ? vk::PipelineStageFlagBits::eTopOfPipe
                : vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader);
        rawOutputLayouts[safeFrameIndex] = vk::ImageLayout::eGeneral;

        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::Pipeline>(*farPipeline.pipeline));
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::PipelineLayout>(*farPipeline.layout),
            0,
            descriptorSetHandle(farDescriptorSets[safeFrameIndex]),
            nullptr);
        commandBuffer.dispatch(groupCountX, groupCountY, 1);

        transitionImage(
            commandBuffer,
            rawOutputImages[safeFrameIndex].handle(),
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eShaderWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader);
        rawOutputLayouts[safeFrameIndex] = vk::ImageLayout::eShaderReadOnlyOptimal;

        if (historyLayouts[safeFrameIndex] == vk::ImageLayout::eUndefined)
        {
            transitionImage(
                commandBuffer,
                historyImages[safeFrameIndex].handle(),
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                {},
                vk::AccessFlagBits::eShaderRead,
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eComputeShader);
            historyLayouts[safeFrameIndex] = vk::ImageLayout::eShaderReadOnlyOptimal;
        }

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
            static_cast<vk::Pipeline>(*reprojectPipeline.pipeline));
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            static_cast<vk::PipelineLayout>(*reprojectPipeline.layout),
            0,
            descriptorSetHandle(reprojectDescriptorSets[safeFrameIndex]),
            nullptr);
        commandBuffer.dispatch(groupCountX, groupCountY, 1);

        transitionImage(
            commandBuffer,
            outputImages[safeFrameIndex].handle(),
            vk::ImageLayout::eGeneral,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::AccessFlagBits::eShaderWrite,
            vk::AccessFlagBits::eTransferRead,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eTransfer);
        outputLayouts[safeFrameIndex] = vk::ImageLayout::eTransferSrcOptimal;

        transitionImage(
            commandBuffer,
            historyImages[safeFrameIndex].handle(),
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageLayout::eTransferDstOptimal,
            vk::AccessFlagBits::eShaderRead,
            vk::AccessFlagBits::eTransferWrite,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eTransfer);
        historyLayouts[safeFrameIndex] = vk::ImageLayout::eTransferDstOptimal;

        const vk::ImageCopy copyRegion{
            .srcSubresource = vk::ImageSubresourceLayers{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
            .dstSubresource = vk::ImageSubresourceLayers{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
            .extent = vk::Extent3D{
                .width = renderExtent.width,
                .height = renderExtent.height,
                .depth = 1,
            },
        };
        commandBuffer.copyImage(
            outputImages[safeFrameIndex].handle(),
            vk::ImageLayout::eTransferSrcOptimal,
            historyImages[safeFrameIndex].handle(),
            vk::ImageLayout::eTransferDstOptimal,
            copyRegion);

        transitionImage(
            commandBuffer,
            historyImages[safeFrameIndex].handle(),
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader);
        historyLayouts[safeFrameIndex] = vk::ImageLayout::eShaderReadOnlyOptimal;

        transitionImage(
            commandBuffer,
            outputImages[safeFrameIndex].handle(),
            vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eTransferRead,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader);
        outputLayouts[safeFrameIndex] = vk::ImageLayout::eShaderReadOnlyOptimal;
        historyViewProjections[safeFrameIndex] = currentViewProjection;
        historyValid[safeFrameIndex] = true;

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

    auto CloudNubisCubedPass::drawGui(CloudNubisCubedSettings& settings) -> void
    {
#if defined(AERKANIS_IMGUI)
        if (ImGui::CollapsingHeader("Cloud Nubis3", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enabled", &settings.enabled);
            ImGui::Checkbox("Light Grid", &settings.lightGridEnabled);
            ImGui::Checkbox("Near/Far Split", &settings.nearFarSplitEnabled);
            ImGui::Checkbox("Temporal Reprojection", &settings.temporalReprojectionEnabled);
            const char* datasetNames[] = {"Parkour", "Storm Bird"};
            ImGui::Combo("Dataset", &settings.datasetIndex, datasetNames, 2);
            ImGui::SliderInt("Ray Steps", &settings.stepCount, 16, 192);
            ImGui::SliderInt("Shadow Steps", &settings.shadowStepCount, 0, 16);
            ImGui::DragFloat("Voxel Scale", &settings.voxelScale, 0.01F, 0.1F, 12.0F, "%.2f");
            ImGui::DragFloat("Near Split", &settings.nearSplitDistance, 0.05F, 0.0F, 512.0F, "%.2f");
            ImGui::SliderFloat("Density", &settings.densityMultiplier, 0.0F, 8.0F, "%.2f");
            ImGui::SliderFloat("Detail Scale", &settings.detailScale, 0.1F, 16.0F, "%.2f");
            ImGui::SliderFloat("Detail Strength", &settings.detailStrength, 0.0F, 2.0F, "%.2f");
            ImGui::DragFloat("Far Clip", &settings.farClip, 0.5F, 1.0F, 512.0F, "%.1f");
            ImGui::SliderFloat("Transmittance Limit", &settings.transmittanceLimit, 0.001F, 0.5F, "%.3f");
            ImGui::SliderFloat("Temporal Blend", &settings.temporalBlend, 0.0F, 0.95F, "%.2f");
            ImGui::DragFloat("Wind Speed", &settings.windSpeed, 0.001F, -4.0F, 4.0F, "%.3f");
            ImGui::DragFloat2("Wind Direction", &settings.windDirection.x, 0.01F, -1.0F, 1.0F, "%.2f");
            ImGui::SliderFloat("Phase G", &settings.phaseG, -0.95F, 0.95F, "%.2f");
            ImGui::SliderFloat("Powder", &settings.powderStrength, 0.0F, 4.0F, "%.2f");
            ImGui::SliderFloat("Absorption", &settings.absorption, 0.0F, 8.0F, "%.2f");
            ImGui::SliderFloat("Exposure", &settings.exposure, 0.01F, 8.0F, "%.2f");
        }
#endif

        settings.sanitize();
    }

    auto CloudNubisCubedPass::loadResources() -> void
    {
        if (resourcesLoaded)
        {
            return;
        }

        const std::filesystem::path cloudData = findCloudAssetPath("cloud-data");
        const std::filesystem::path nubis3Data = cloudData / "vdb-for-nubis3";

        modelingTexture0 = uploader.uploadVolumeSlices(
            nubis3Data / "example1",
            "modeling_data(",
            ").tga",
            64);
        modelingTexture1 = uploader.uploadVolumeSlices(
            nubis3Data / "example2",
            "modeling_data(",
            ").tga",
            64);
        detailNoiseTexture = uploader.uploadVolumeSlices(
            cloudData / "noise",
            "NubisVoxelCloudNoise(",
            ").tga",
            128);
        resourcesLoaded = true;
    }

    auto CloudNubisCubedPass::createParameterBuffers(std::size_t frameCount) -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Nubis3 parameter buffer requires a context");
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
                    .size = static_cast<vk::DeviceSize>(sizeof(CloudNubisCubedParameters)),
                    .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                    .memoryProperties =
                        vk::MemoryPropertyFlagBits::eHostVisible |
                        vk::MemoryPropertyFlagBits::eHostCoherent,
                });
        }
    }

    auto CloudNubisCubedPass::createOutputs(vk::Extent2D extent) -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Nubis3 output requires a context");
        }
        if (extent.width == 0 || extent.height == 0)
        {
            throw std::runtime_error("Nubis3 output requires a non-zero extent");
        }

        const vk::Format cloudFormat = vk::Format::eR16G16B16A16Sfloat;
        const std::size_t frameCount = std::max<std::size_t>(parameterBuffers.size(), 1);

        lightGridImage.shutdown();
        lightGridImage.init(
            *context,
            Vk::ImageConfig{
                .imageType = vk::ImageType::e3D,
                .format = cloudFormat,
                .extent = vk::Extent3D{
                    .width = NubisCubedLightGridHorizontalSize,
                    .height = NubisCubedLightGridHorizontalSize,
                    .depth = NubisCubedLightGridVerticalSize,
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
                    .format = cloudFormat,
                    .viewType = vk::ImageViewType::e3D,
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .levelCount = 1,
                    .layerCount = 1,
                },
            });
        lightGridLayout = vk::ImageLayout::eUndefined;

        auto resetImages = [](std::vector<Vk::Image>& images) {
            for (Vk::Image& image : images)
            {
                image.shutdown();
            }
            images.clear();
        };
        resetImages(nearStateImages);
        resetImages(rawOutputImages);
        resetImages(outputImages);
        resetImages(historyImages);

        auto createFrameImage = [&](Vk::Image& image, vk::ImageUsageFlags usage) {
            image.init(
                *context,
                Vk::ImageConfig{
                    .imageType = vk::ImageType::e2D,
                    .format = cloudFormat,
                    .extent = vk::Extent3D{
                        .width = extent.width,
                        .height = extent.height,
                        .depth = 1,
                    },
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = vk::SampleCountFlagBits::e1,
                    .tiling = vk::ImageTiling::eOptimal,
                    .usage = usage,
                    .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                    .createView = true,
                    .view = Vk::ImageViewConfig{
                        .format = cloudFormat,
                        .viewType = vk::ImageViewType::e2D,
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                });
        };

        nearStateImages.resize(frameCount);
        rawOutputImages.resize(frameCount);
        outputImages.resize(frameCount);
        historyImages.resize(frameCount);
        for (std::size_t index = 0; index < frameCount; ++index)
        {
            createFrameImage(
                nearStateImages[index],
                vk::ImageUsageFlagBits::eStorage |
                    vk::ImageUsageFlagBits::eSampled);
            createFrameImage(
                rawOutputImages[index],
                vk::ImageUsageFlagBits::eStorage |
                    vk::ImageUsageFlagBits::eSampled);
            createFrameImage(
                outputImages[index],
                vk::ImageUsageFlagBits::eStorage |
                    vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eTransferSrc);
            createFrameImage(
                historyImages[index],
                vk::ImageUsageFlagBits::eSampled |
                    vk::ImageUsageFlagBits::eTransferDst);
        }

        nearStateLayouts.assign(frameCount, vk::ImageLayout::eUndefined);
        rawOutputLayouts.assign(frameCount, vk::ImageLayout::eUndefined);
        outputLayouts.assign(frameCount, vk::ImageLayout::eUndefined);
        historyLayouts.assign(frameCount, vk::ImageLayout::eUndefined);
        historyViewProjections.assign(frameCount, glm::mat4{1.0F});
        historyValid.assign(frameCount, false);
        outputSampler = makeLinearClampSampler(context->device);
    }

    auto CloudNubisCubedPass::createDescriptors() -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Nubis3 descriptors require a context");
        }

        lightGridSetLayout.shutdown();
        nearSetLayout.shutdown();
        farSetLayout.shutdown();
        reprojectSetLayout.shutdown();
        compositeSetLayout.shutdown();

        lightGridSetLayout
            .addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute)
            .addBinding(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(2, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(3, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(4, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eCompute)
            .build(context->device);

        nearSetLayout
            .addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute)
            .addBinding(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(2, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(3, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(4, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(5, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(6, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eCompute)
            .build(context->device);

        farSetLayout
            .addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute)
            .addBinding(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(2, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(3, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(4, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(5, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(6, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(7, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eCompute)
            .build(context->device);

        reprojectSetLayout
            .addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute)
            .addBinding(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(2, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(3, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eCompute)
            .addBinding(4, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eCompute)
            .build(context->device);

        compositeSetLayout
            .addBinding(0, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .addBinding(1, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment)
            .build(context->device);

        const uint32_t frameSetCount = static_cast<uint32_t>(std::max<std::size_t>(parameterBuffers.size(), 1));
        const std::array poolSizes{
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eStorageImage,
                .descriptorCount = frameSetCount * 4U,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = frameSetCount * 4U,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampledImage,
                .descriptorCount = frameSetCount * 14U,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampler,
                .descriptorCount = frameSetCount * 5U,
            },
        };

        descriptorPool.init(
            context->device,
            frameSetCount * 5U,
            poolSizes);
        lightGridDescriptorSets.clear();
        lightGridDescriptorSets.reserve(parameterBuffers.size());
        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            lightGridDescriptorSets.emplace_back(descriptorPool.allocate(context->device, lightGridSetLayout));
        }
        nearDescriptorSets.clear();
        nearDescriptorSets.reserve(parameterBuffers.size());
        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            nearDescriptorSets.emplace_back(descriptorPool.allocate(context->device, nearSetLayout));
        }
        farDescriptorSets.clear();
        farDescriptorSets.reserve(parameterBuffers.size());
        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            farDescriptorSets.emplace_back(descriptorPool.allocate(context->device, farSetLayout));
        }
        reprojectDescriptorSets.clear();
        reprojectDescriptorSets.reserve(parameterBuffers.size());
        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            reprojectDescriptorSets.emplace_back(descriptorPool.allocate(context->device, reprojectSetLayout));
        }
        compositeDescriptorSets.clear();
        compositeDescriptorSets.reserve(parameterBuffers.size());
        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            compositeDescriptorSets.emplace_back(descriptorPool.allocate(context->device, compositeSetLayout));
        }
    }

    auto CloudNubisCubedPass::updateDescriptors() -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Nubis3 descriptor update requires a context");
        }
        if (lightGridDescriptorSets.size() != parameterBuffers.size() ||
            nearDescriptorSets.size() != parameterBuffers.size() ||
            farDescriptorSets.size() != parameterBuffers.size() ||
            reprojectDescriptorSets.size() != parameterBuffers.size() ||
            nearStateImages.size() < parameterBuffers.size() ||
            rawOutputImages.size() < parameterBuffers.size() ||
            outputImages.size() < parameterBuffers.size() ||
            historyImages.size() < parameterBuffers.size() ||
            outputImages.size() < compositeDescriptorSets.size())
        {
            throw std::runtime_error("Nubis3 descriptor update requires per-frame output images");
        }

        Vk::DescriptorWriter writer{};
        const vk::DescriptorImageInfo lightGridSampleInfo{
            .imageView = lightGridImage.viewHandle(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        const vk::DescriptorImageInfo samplerInfo{
            .sampler = static_cast<vk::Sampler>(*detailNoiseTexture.sampler),
        };
        const vk::DescriptorImageInfo outputSamplerInfo{
            .sampler = static_cast<vk::Sampler>(*outputSampler),
        };

        for (std::size_t index = 0; index < parameterBuffers.size(); ++index)
        {
            writer.writeBuffer(
                    descriptorSetHandle(lightGridDescriptorSets[index]),
                    0,
                    vk::DescriptorType::eUniformBuffer,
                    parameterBuffers[index].descriptorInfo())
                .writeImage(
                    descriptorSetHandle(lightGridDescriptorSets[index]),
                    1,
                    vk::DescriptorType::eStorageImage,
                    vk::DescriptorImageInfo{
                        .imageView = lightGridImage.viewHandle(),
                        .imageLayout = vk::ImageLayout::eGeneral,
                    })
                .writeImage(
                    descriptorSetHandle(lightGridDescriptorSets[index]),
                    2,
                    vk::DescriptorType::eSampledImage,
                    modelingTexture0.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(lightGridDescriptorSets[index]),
                    3,
                    vk::DescriptorType::eSampledImage,
                    modelingTexture1.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(lightGridDescriptorSets[index]),
                    4,
                    vk::DescriptorType::eSampler,
                    samplerInfo);

            writer.writeBuffer(
                    descriptorSetHandle(nearDescriptorSets[index]),
                    0,
                    vk::DescriptorType::eUniformBuffer,
                    parameterBuffers[index].descriptorInfo())
                .writeImage(
                    descriptorSetHandle(nearDescriptorSets[index]),
                    1,
                    vk::DescriptorType::eStorageImage,
                    vk::DescriptorImageInfo{
                        .imageView = nearStateImages[index].viewHandle(),
                        .imageLayout = vk::ImageLayout::eGeneral,
                    })
                .writeImage(
                    descriptorSetHandle(nearDescriptorSets[index]),
                    2,
                    vk::DescriptorType::eSampledImage,
                    modelingTexture0.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(nearDescriptorSets[index]),
                    3,
                    vk::DescriptorType::eSampledImage,
                    modelingTexture1.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(nearDescriptorSets[index]),
                    4,
                    vk::DescriptorType::eSampledImage,
                    detailNoiseTexture.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(nearDescriptorSets[index]),
                    5,
                    vk::DescriptorType::eSampledImage,
                    lightGridSampleInfo)
                .writeImage(
                    descriptorSetHandle(nearDescriptorSets[index]),
                    6,
                    vk::DescriptorType::eSampler,
                    samplerInfo);

            writer.writeBuffer(
                    descriptorSetHandle(farDescriptorSets[index]),
                    0,
                    vk::DescriptorType::eUniformBuffer,
                    parameterBuffers[index].descriptorInfo())
                .writeImage(
                    descriptorSetHandle(farDescriptorSets[index]),
                    1,
                    vk::DescriptorType::eStorageImage,
                    vk::DescriptorImageInfo{
                        .imageView = rawOutputImages[index].viewHandle(),
                        .imageLayout = vk::ImageLayout::eGeneral,
                    })
                .writeImage(
                    descriptorSetHandle(farDescriptorSets[index]),
                    2,
                    vk::DescriptorType::eSampledImage,
                    vk::DescriptorImageInfo{
                        .imageView = nearStateImages[index].viewHandle(),
                        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    })
                .writeImage(
                    descriptorSetHandle(farDescriptorSets[index]),
                    3,
                    vk::DescriptorType::eSampledImage,
                    modelingTexture0.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(farDescriptorSets[index]),
                    4,
                    vk::DescriptorType::eSampledImage,
                    modelingTexture1.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(farDescriptorSets[index]),
                    5,
                    vk::DescriptorType::eSampledImage,
                    detailNoiseTexture.descriptorInfo())
                .writeImage(
                    descriptorSetHandle(farDescriptorSets[index]),
                    6,
                    vk::DescriptorType::eSampledImage,
                    lightGridSampleInfo)
                .writeImage(
                    descriptorSetHandle(farDescriptorSets[index]),
                    7,
                    vk::DescriptorType::eSampler,
                    samplerInfo);

            writer.writeBuffer(
                    descriptorSetHandle(reprojectDescriptorSets[index]),
                    0,
                    vk::DescriptorType::eUniformBuffer,
                    parameterBuffers[index].descriptorInfo())
                .writeImage(
                    descriptorSetHandle(reprojectDescriptorSets[index]),
                    1,
                    vk::DescriptorType::eStorageImage,
                    vk::DescriptorImageInfo{
                        .imageView = outputImages[index].viewHandle(),
                        .imageLayout = vk::ImageLayout::eGeneral,
                    })
                .writeImage(
                    descriptorSetHandle(reprojectDescriptorSets[index]),
                    2,
                    vk::DescriptorType::eSampledImage,
                    vk::DescriptorImageInfo{
                        .imageView = rawOutputImages[index].viewHandle(),
                        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    })
                .writeImage(
                    descriptorSetHandle(reprojectDescriptorSets[index]),
                    3,
                    vk::DescriptorType::eSampledImage,
                    vk::DescriptorImageInfo{
                        .imageView = historyImages[index].viewHandle(),
                        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                    })
                .writeImage(
                    descriptorSetHandle(reprojectDescriptorSets[index]),
                    4,
                    vk::DescriptorType::eSampler,
                    outputSamplerInfo);
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
                    outputSamplerInfo);
        }
        writer.update(context->device);
    }

    auto CloudNubisCubedPass::createPipelines(vk::Format swapchainFormat) -> void
    {
        if (context == nullptr)
        {
            throw std::runtime_error("Nubis3 pipelines require a context");
        }

        lightGridShader.shutdown();
        nearShader.shutdown();
        farShader.shutdown();
        reprojectShader.shutdown();
        compositeShader.shutdown();
        lightGridShader.init(context->device, shaderPath("cloud/cloud-nubis-cubed-light-grid.spv"));
        nearShader.init(context->device, shaderPath("cloud/cloud-nubis-cubed-near.spv"));
        farShader.init(context->device, shaderPath("cloud/cloud-nubis-cubed-far.spv"));
        reprojectShader.init(context->device, shaderPath("cloud/cloud-nubis-cubed-reproject.spv"));
        compositeShader.init(context->device, shaderPath("cloud/cloud-composite.spv"));

        Vk::PipelineBuilder lightGridBuilder{};
        lightGridBuilder.addShaderStage(lightGridShader, vk::ShaderStageFlagBits::eCompute, "compMain")
            .addDescriptorSetLayout(lightGridSetLayout);
        lightGridPipeline = lightGridBuilder.buildCompute(context->device);

        Vk::PipelineBuilder nearBuilder{};
        nearBuilder.addShaderStage(nearShader, vk::ShaderStageFlagBits::eCompute, "compMain")
            .addDescriptorSetLayout(nearSetLayout);
        nearPipeline = nearBuilder.buildCompute(context->device);

        Vk::PipelineBuilder farBuilder{};
        farBuilder.addShaderStage(farShader, vk::ShaderStageFlagBits::eCompute, "compMain")
            .addDescriptorSetLayout(farSetLayout);
        farPipeline = farBuilder.buildCompute(context->device);

        Vk::PipelineBuilder reprojectBuilder{};
        reprojectBuilder.addShaderStage(reprojectShader, vk::ShaderStageFlagBits::eCompute, "compMain")
            .addDescriptorSetLayout(reprojectSetLayout);
        reprojectPipeline = reprojectBuilder.buildCompute(context->device);

        Vk::PipelineBuilder compositeBuilder{};
        compositeBuilder.addShaderStage(compositeShader, vk::ShaderStageFlagBits::eVertex, "vertMain")
            .addShaderStage(compositeShader, vk::ShaderStageFlagBits::eFragment, "fragMain")
            .addDescriptorSetLayout(compositeSetLayout)
            .setColorAttachmentFormat(swapchainFormat)
            .setCullMode(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
            .disableDepth();
        compositePipeline = compositeBuilder.buildGraphics(context->device);
    }

    auto CloudNubisCubedPass::shaderPath(std::filesystem::path const& relativePath) const
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

    auto CloudNubisCubedPass::elapsedSeconds() const -> float
    {
        if (startTime.time_since_epoch().count() == 0)
        {
            return 0.0F;
        }

        return std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
    }

}  // namespace Aerkanis::Cloud
