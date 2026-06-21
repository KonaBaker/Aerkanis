#include "render/render.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#if defined(AERKANIS_IMGUI)
#include "imgui.h"
#endif

namespace Aerkanis
{

    namespace
    {

        struct TrianglePushConstants
        {
            glm::vec4 viewProjectionRow0{};
            glm::vec4 viewProjectionRow1{};
            glm::vec4 viewProjectionRow2{};
            glm::vec4 viewProjectionRow3{};
            glm::vec4 triangle{};
        };

        static_assert(sizeof(TrianglePushConstants) == sizeof(glm::vec4) * 5);

        auto shaderPath(std::filesystem::path const& relativePath) -> std::filesystem::path
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

        auto matrixRow(glm::mat4 const& matrix, int rowIndex) -> glm::vec4
        {
            return glm::vec4{
                matrix[0][rowIndex],
                matrix[1][rowIndex],
                matrix[2][rowIndex],
                matrix[3][rowIndex],
            };
        }

        auto makeTrianglePushConstants(Scene::SceneState const& sceneState, vk::Extent2D extent)
            -> TrianglePushConstants
        {
            const float aspectRatio =
                extent.height == 0
                    ? 1.0F
                    : static_cast<float>(extent.width) / static_cast<float>(extent.height);
            const glm::mat4 viewProjection = sceneState.camera.viewProjection(aspectRatio);

            return TrianglePushConstants{
                .viewProjectionRow0 = matrixRow(viewProjection, 0),
                .viewProjectionRow1 = matrixRow(viewProjection, 1),
                .viewProjectionRow2 = matrixRow(viewProjection, 2),
                .viewProjectionRow3 = matrixRow(viewProjection, 3),
                .triangle = glm::vec4{
                    sceneState.triangle.size,
                    sceneState.triangle.rotationRadians,
                    0.0F,
                    0.0F,
                },
            };
        }

        auto waitForDrawableFramebuffer(Window const& window) -> bool
        {
            int width = 0;
            int height = 0;
            window.getFramebufferSize(width, height);

            while (!window.shouldClose() && (width == 0 || height == 0))
            {
                glfwWaitEvents();
                window.getFramebufferSize(width, height);
            }

            return !window.shouldClose();
        }

        auto swapchainColorRange() noexcept -> vk::ImageSubresourceRange
        {
            return vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            };
        }

        auto transitionSwapchainImage(
            vk::raii::CommandBuffer const& commandBuffer,
            vk::Image image,
            vk::ImageLayout oldLayout,
            vk::ImageLayout newLayout,
            vk::AccessFlags srcAccess,
            vk::AccessFlags dstAccess,
            vk::PipelineStageFlags srcStage,
            vk::PipelineStageFlags dstStage) -> void
        {
            const vk::ImageMemoryBarrier barrier{
                .srcAccessMask = srcAccess,
                .dstAccessMask = dstAccess,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = swapchainColorRange(),
            };

            commandBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, {}, barrier);
        }

        auto clearTarget(
            vk::raii::CommandBuffer const& commandBuffer,
            vk::ImageView targetView,
            vk::ImageLayout targetLayout,
            vk::Extent2D extent,
            std::array<float, 4> color) -> void
        {
            vk::ClearColorValue clearColor{};
            clearColor.float32 = color;

            vk::ClearValue clearValue{};
            clearValue.color = clearColor;

            const vk::RenderingAttachmentInfo colorAttachment{
                .imageView = targetView,
                .imageLayout = targetLayout,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = clearValue,
            };

            const vk::RenderingInfo renderingInfo{
                .renderArea = vk::Rect2D{
                    .offset = vk::Offset2D{.x = 0, .y = 0},
                    .extent = extent,
                },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachment,
            };

            commandBuffer.beginRendering(renderingInfo);
            commandBuffer.endRendering();
        }

        auto setFullscreenViewport(
            vk::raii::CommandBuffer const& commandBuffer,
            vk::Extent2D extent) -> void
        {
            const vk::Viewport viewport{
                .x = 0.0F,
                .y = 0.0F,
                .width = static_cast<float>(extent.width),
                .height = static_cast<float>(extent.height),
                .minDepth = 0.0F,
                .maxDepth = 1.0F,
            };
            const vk::Rect2D scissor{
                .offset = vk::Offset2D{.x = 0, .y = 0},
                .extent = extent,
            };

            commandBuffer.setViewport(0, viewport);
            commandBuffer.setScissor(0, scissor);
        }

        auto isUsableAcquireResult(vk::Result result) noexcept -> bool
        {
            return result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR;
        }

    }  // namespace

    auto Render::init(Window const& window) -> bool
    {
        shutdown();

        try
        {
            context.init(window);
            if (static_cast<vk::Device>(*context.device) == VK_NULL_HANDLE)
            {
                throw std::runtime_error("Vulkan context did not create a logical device");
            }

            if (!waitForDrawableFramebuffer(window))
            {
                return false;
            }

            swapchain.init(context, context.surface, window);
            swapchainImageInitialized.assign(swapchain.images.size(), false);

            if (!frames.init(context))
            {
                throw std::runtime_error("failed to initialize frame data");
            }

            triangleShader.init(context.device, shaderPath("triangle.spv"));
            createTrianglePipeline();
            if (!environmentPass.init(context, swapchain.imageFormat, swapchain.extent, frames.size()))
            {
                std::cerr << "[Environment] Sun sky pass disabled\n";
            }
            if (!cloudNubisPass.init(context, swapchain.imageFormat, swapchain.extent, frames.size()))
            {
                std::cerr << "[Cloud] Nubis pass disabled\n";
            }
#if defined(AERKANIS_IMGUI)
            if (!guiPass.init(window, context, swapchain))
            {
                std::cerr << "[ImGui] GUI disabled\n";
            }
#endif

            previousFrameTime = std::chrono::steady_clock::now();
            initialized = true;
            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[Vulkan] render init failed: " << exception.what() << '\n';
            shutdown();
            return false;
        }
    }

    auto Render::shutdown() noexcept -> void
    {
        try
        {
            if (static_cast<vk::Device>(*context.device) != VK_NULL_HANDLE)
            {
                context.device.waitIdle();
            }
        }
        catch (...)
        {
        }

        trianglePipeline.pipeline.clear();
        trianglePipeline.layout.clear();
        guiPass.shutdown();
        cloudNubisPass.shutdown();
        environmentPass.shutdown();
        triangleShader.shutdown();
        frames.shutdown();
        swapchain.cleanup();
        swapchainImageInitialized.clear();
        context.shutdown();
        previousFrameTime = {};
        initialized = false;
    }

    auto Render::drawFrame(Window const& window, bool& framebufferResized) -> bool
    {
        if (!initialized)
        {
            return false;
        }

        try
        {
            if (framebufferResized)
            {
                framebufferResized = false;
                return recreateSwapchain(window);
            }

            FrameData& currentFrame = frames.frame();
            const vk::Fence inFlightFence = static_cast<vk::Fence>(*currentFrame.inFlight);
            static_cast<void>(context.device.waitForFences(inFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));

            auto [acquireResult, imageIndex] = swapchain.swapchain.acquireNextImage(
                std::numeric_limits<uint64_t>::max(),
                static_cast<vk::Semaphore>(*currentFrame.imageAvailable));

            if (acquireResult == vk::Result::eErrorOutOfDateKHR)
            {
                return recreateSwapchain(window);
            }
            if (!isUsableAcquireResult(acquireResult))
            {
                throw std::runtime_error("failed to acquire swapchain image");
            }

            context.device.resetFences(inFlightFence);

            const auto currentTime = std::chrono::steady_clock::now();
            float deltaSeconds = std::chrono::duration<float>(currentTime - previousFrameTime).count();
            previousFrameTime = currentTime;
            deltaSeconds = std::clamp(deltaSeconds, 0.0F, 0.1F);

            if (!currentFrame.begin() ||
                !recordCommands(currentFrame, imageIndex, window, deltaSeconds) ||
                !currentFrame.end())
            {
                return false;
            }

            const vk::Semaphore waitSemaphore = static_cast<vk::Semaphore>(*currentFrame.imageAvailable);
            const vk::Semaphore signalSemaphore = static_cast<vk::Semaphore>(*currentFrame.renderFinished);
            const vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            const vk::CommandBuffer commandBuffer = static_cast<vk::CommandBuffer>(*currentFrame.commandBuffer);

            const vk::SubmitInfo submitInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &waitSemaphore,
                .pWaitDstStageMask = &waitStage,
                .commandBufferCount = 1,
                .pCommandBuffers = &commandBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &signalSemaphore,
            };
            context.graphicsQueue.submit(submitInfo, inFlightFence);

            const vk::SwapchainKHR rawSwapchain = static_cast<vk::SwapchainKHR>(*swapchain.swapchain);
            const vk::PresentInfoKHR presentInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &signalSemaphore,
                .swapchainCount = 1,
                .pSwapchains = &rawSwapchain,
                .pImageIndices = &imageIndex,
            };

            const vk::Result presentResult = context.presentQueue.presentKHR(presentInfo);
            frames.advance();

            if (presentResult == vk::Result::eErrorOutOfDateKHR ||
                presentResult == vk::Result::eSuboptimalKHR ||
                framebufferResized)
            {
                framebufferResized = false;
                return recreateSwapchain(window);
            }
            if (presentResult != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to present swapchain image");
            }

            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[Vulkan] draw frame failed: " << exception.what() << '\n';
            return false;
        }
    }

    auto Render::createTrianglePipeline() -> void
    {
        trianglePipeline.pipeline.clear();
        trianglePipeline.layout.clear();

        Vk::PipelineBuilder builder{};
        builder.addShaderStage(triangleShader, vk::ShaderStageFlagBits::eVertex, "vertMain")
            .addShaderStage(triangleShader, vk::ShaderStageFlagBits::eFragment, "fragMain")
            .setColorAttachmentFormat(swapchain.imageFormat)
            .setCullMode(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
            .disableDepth()
            .addPushConstantRange(vk::PushConstantRange{
                .stageFlags = vk::ShaderStageFlagBits::eVertex,
                .offset = 0,
                .size = static_cast<uint32_t>(sizeof(TrianglePushConstants)),
            });

        trianglePipeline = builder.buildGraphics(context.device);
    }

    auto Render::recreateSwapchain(Window const& window) -> bool
    {
        if (!waitForDrawableFramebuffer(window))
        {
            return false;
        }

        context.device.waitIdle();
        guiPass.shutdown();
        trianglePipeline.pipeline.clear();
        trianglePipeline.layout.clear();
        swapchain.recreate(context, context.surface, window);
        swapchainImageInitialized.assign(swapchain.images.size(), false);
        createTrianglePipeline();

        if (environmentPass.initialized)
        {
            if (!environmentPass.recreate(swapchain.imageFormat, swapchain.extent))
            {
                std::cerr << "[Environment] Sun sky pass disabled after swapchain recreation\n";
            }
        }
        else if (!environmentPass.init(context, swapchain.imageFormat, swapchain.extent, frames.size()))
        {
            std::cerr << "[Environment] Sun sky pass disabled after swapchain recreation\n";
        }

        if (cloudNubisPass.initialized)
        {
            if (!cloudNubisPass.recreate(swapchain.imageFormat, swapchain.extent))
            {
                std::cerr << "[Cloud] Nubis pass disabled after swapchain recreation\n";
            }
        }
#if defined(AERKANIS_IMGUI)
        if (!guiPass.init(window, context, swapchain))
        {
            std::cerr << "[ImGui] GUI disabled after swapchain recreation\n";
        }
#endif
        return true;
    }

    auto Render::recordCommands(
        FrameData& frame,
        uint32_t imageIndex,
        Window const& window,
        float deltaSeconds) -> bool
    {
        if (imageIndex >= swapchain.images.size() || imageIndex >= swapchain.imageViews.size())
        {
            std::cerr << "[Vulkan] swapchain image index out of range\n";
            return false;
        }

        vk::raii::CommandBuffer const& commandBuffer = frame.commandBuffer;
        guiPass.beginFrame();
        settings.sanitize();
        sunSkySettings.sanitize();
        cameraController.update(
            window,
            sceneState.camera,
            deltaSeconds,
            guiPass.wantsMouseCapture(),
            guiPass.wantsKeyboardCapture());
        guiPass.drawSceneControls(sceneState, settings.pipeline == RenderPipeline::TriangleDemo);
        drawRenderControls();

        const Environment::SunSkyState sunSky = Environment::makeSunSkyState(sunSkySettings);
        const vk::ImageLayout oldLayout =
            swapchainImageInitialized[imageIndex]
                ? vk::ImageLayout::ePresentSrcKHR
                : vk::ImageLayout::eUndefined;

        transitionSwapchainImage(
            commandBuffer,
            swapchain.images[imageIndex],
            oldLayout,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},
            vk::AccessFlagBits::eColorAttachmentWrite,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eColorAttachmentOutput);

        switch (settings.pipeline)
        {
        case RenderPipeline::TriangleDemo:
            recordTriangleDemo(commandBuffer, imageIndex);
            break;
        case RenderPipeline::SunCloudNubis:
        case RenderPipeline::SunCloudNubisCubed:
            recordSunCloud(commandBuffer, imageIndex, sunSky);
            break;
        }

        const vk::RenderingAttachmentInfo loadColorAttachment{
            .imageView = static_cast<vk::ImageView>(*swapchain.imageViews[imageIndex]),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        const vk::RenderingInfo loadRenderingInfo{
            .renderArea = vk::Rect2D{
                .offset = vk::Offset2D{.x = 0, .y = 0},
                .extent = swapchain.extent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &loadColorAttachment,
        };

        commandBuffer.beginRendering(loadRenderingInfo);
        guiPass.render(commandBuffer);
        commandBuffer.endRendering();

        transitionSwapchainImage(
            commandBuffer,
            swapchain.images[imageIndex],
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits::eColorAttachmentWrite,
            {},
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eBottomOfPipe);

        swapchainImageInitialized[imageIndex] = true;
        return true;
    }

    auto Render::recordTriangleDemo(vk::raii::CommandBuffer const& commandBuffer, uint32_t imageIndex) -> void
    {
        vk::ClearColorValue clearColor{};
        clearColor.float32 = std::array<float, 4>{0.015F, 0.018F, 0.026F, 1.0F};

        vk::ClearValue clearValue{};
        clearValue.color = clearColor;

        const vk::RenderingAttachmentInfo colorAttachment{
            .imageView = static_cast<vk::ImageView>(*swapchain.imageViews[imageIndex]),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearValue,
        };

        const vk::RenderingInfo renderingInfo{
            .renderArea = vk::Rect2D{
                .offset = vk::Offset2D{.x = 0, .y = 0},
                .extent = swapchain.extent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
        };

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(
            vk::PipelineBindPoint::eGraphics,
            static_cast<vk::Pipeline>(*trianglePipeline.pipeline));
        setFullscreenViewport(commandBuffer, swapchain.extent);

        const TrianglePushConstants pushConstants = makeTrianglePushConstants(sceneState, swapchain.extent);
        commandBuffer.pushConstants(
            static_cast<vk::PipelineLayout>(*trianglePipeline.layout),
            vk::ShaderStageFlagBits::eVertex,
            0,
            vk::ArrayProxy<const TrianglePushConstants>{pushConstants});
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRendering();
    }

    auto Render::recordSunCloud(
        vk::raii::CommandBuffer const& commandBuffer,
        uint32_t imageIndex,
        Environment::SunSkyState const& sunSky) -> void
    {
        const vk::ImageView targetView = static_cast<vk::ImageView>(*swapchain.imageViews[imageIndex]);
        if (environmentPass.initialized)
        {
            environmentPass.record(
                commandBuffer,
                targetView,
                vk::ImageLayout::eColorAttachmentOptimal,
                sunSky,
                sceneState.camera,
                frames.currentFrame);
        }
        else
        {
            clearTarget(
                commandBuffer,
                targetView,
                vk::ImageLayout::eColorAttachmentOptimal,
                swapchain.extent,
                std::array<float, 4>{0.015F, 0.018F, 0.026F, 1.0F});
        }

        if (settings.pipeline == RenderPipeline::SunCloudNubis)
        {
            cloudNubisPass.record(
                commandBuffer,
                targetView,
                swapchain.images[imageIndex],
                vk::ImageLayout::eColorAttachmentOptimal,
                sunSky,
                sceneState.camera,
                frames.currentFrame);
        }
    }

    auto Render::drawRenderControls() -> void
    {
#if defined(AERKANIS_IMGUI)
        if (guiPass.initialized && guiPass.imguiContext != nullptr)
        {
            ImGui::SetCurrentContext(guiPass.imguiContext);
            ImGui::SetNextWindowPos(ImVec2{374.0F, 16.0F}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2{360.0F, 0.0F}, ImGuiCond_FirstUseEver);

            if (ImGui::Begin("Render"))
            {
                int pipelineIndex = static_cast<int>(settings.pipeline);
                const char* pipelineNames[] = {
                    renderPipelineName(RenderPipeline::TriangleDemo),
                    renderPipelineName(RenderPipeline::SunCloudNubis),
                    renderPipelineName(RenderPipeline::SunCloudNubisCubed),
                };
                if (ImGui::Combo("Pipeline", &pipelineIndex, pipelineNames, 3))
                {
                    settings.pipeline = static_cast<RenderPipeline>(pipelineIndex);
                }

                if (settings.pipeline != RenderPipeline::TriangleDemo)
                {
                    environmentPass.drawGui(sunSkySettings);
                }
                if (settings.pipeline == RenderPipeline::SunCloudNubis)
                {
                    cloudNubisPass.drawGui();
                }
                if (settings.pipeline == RenderPipeline::SunCloudNubisCubed)
                {
                    ImGui::Separator();
                    ImGui::TextUnformatted("Nubis3");
                    ImGui::Checkbox("Enable Reserved Pass", &settings.nubisCubed.enabled);
                    ImGui::SliderInt("Dataset", &settings.nubisCubed.datasetIndex, 0, 16);
                    ImGui::DragFloat("Voxel Scale", &settings.nubisCubed.voxelScale, 0.01F, 0.01F, 64.0F, "%.2f");
                    ImGui::DragFloat(
                        "Density",
                        &settings.nubisCubed.densityMultiplier,
                        0.01F,
                        0.0F,
                        8.0F,
                        "%.2f");
                    ImGui::TextDisabled("Renderer hook reserved for cloud-nubis-cubed-pass.");
                }
            }
            ImGui::End();
        }
#endif

        settings.sanitize();
        sunSkySettings.sanitize();
    }

}  // namespace Aerkanis
