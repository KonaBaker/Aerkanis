#include "core/triangle-renderer.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

#include <GLFW/glfw3.h>

namespace Aerkanis
{

    namespace
    {

        auto shaderPath() -> std::filesystem::path
        {
            const std::array candidates{
                std::filesystem::current_path() / "shaders" / "triangle.spv",
                std::filesystem::current_path() / ".." / "shaders" / "triangle.spv",
                std::filesystem::current_path() / "build" / "shaders" / "triangle.spv",
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

        auto isUsableAcquireResult(vk::Result result) noexcept -> bool
        {
            return result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR;
        }

    }  // namespace

    auto TriangleRenderer::init(Window const& window) -> bool
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

            triangleShader.init(context.device, shaderPath());
            createTrianglePipeline();

            initialized = true;
            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[Vulkan] triangle renderer init failed: " << exception.what() << '\n';
            shutdown();
            return false;
        }
    }

    auto TriangleRenderer::shutdown() noexcept -> void
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
        triangleShader.shutdown();
        frames.shutdown();
        swapchain.cleanup();
        swapchainImageInitialized.clear();
        context.shutdown();
        initialized = false;
    }

    auto TriangleRenderer::drawFrame(Window const& window, bool& framebufferResized) -> bool
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

            if (!currentFrame.begin() || !recordTriangleCommands(currentFrame, imageIndex) || !currentFrame.end())
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

    auto TriangleRenderer::createTrianglePipeline() -> void
    {
        trianglePipeline.pipeline.clear();
        trianglePipeline.layout.clear();

        Vk::PipelineBuilder builder{};
        builder.addShaderStage(triangleShader, vk::ShaderStageFlagBits::eVertex, "vertMain")
            .addShaderStage(triangleShader, vk::ShaderStageFlagBits::eFragment, "fragMain")
            .setColorAttachmentFormat(swapchain.imageFormat)
            .setCullMode(vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise)
            .disableDepth();

        trianglePipeline = builder.buildGraphics(context.device);
    }

    auto TriangleRenderer::recreateSwapchain(Window const& window) -> bool
    {
        if (!waitForDrawableFramebuffer(window))
        {
            return false;
        }

        context.device.waitIdle();
        trianglePipeline.pipeline.clear();
        trianglePipeline.layout.clear();
        swapchain.recreate(context, context.surface, window);
        swapchainImageInitialized.assign(swapchain.images.size(), false);
        createTrianglePipeline();
        return true;
    }

    auto TriangleRenderer::recordTriangleCommands(FrameData& frame, uint32_t imageIndex) -> bool
    {
        if (imageIndex >= swapchain.images.size() || imageIndex >= swapchain.imageViews.size())
        {
            std::cerr << "[Vulkan] swapchain image index out of range\n";
            return false;
        }

        vk::raii::CommandBuffer const& commandBuffer = frame.commandBuffer;
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

        const vk::Viewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(swapchain.extent.width),
            .height = static_cast<float>(swapchain.extent.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F,
        };
        const vk::Rect2D scissor{
            .offset = vk::Offset2D{.x = 0, .y = 0},
            .extent = swapchain.extent,
        };

        commandBuffer.setViewport(0, viewport);
        commandBuffer.setScissor(0, scissor);
        commandBuffer.draw(3, 1, 0, 0);
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

}  // namespace Aerkanis
