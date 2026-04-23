#include "core/frame-data.hpp"

#include <iostream>

namespace Aerkanis
{

    auto FrameData::init(Context& context) -> bool
    {
        shutdown();

        if (!context.queueFamilies.graphicsFamily.has_value())
        {
            std::cerr << "[Vulkan] frame data requires a graphics queue family\n";
            return false;
        }

        try
        {
            vk::CommandPoolCreateInfo commandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = context.queueFamilies.graphicsFamily.value(),
            };

            commandPool = vk::raii::CommandPool(context.device, commandPoolCreateInfo);

            vk::CommandBufferAllocateInfo commandBufferAllocateInfo{
                .commandPool = static_cast<vk::CommandPool>(*commandPool),
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            commandBuffers = vk::raii::CommandBuffers(context.device, commandBufferAllocateInfo);

            vk::SemaphoreCreateInfo semaphoreCreateInfo{};
            imageAvailable = vk::raii::Semaphore(context.device, semaphoreCreateInfo);
            renderFinished = vk::raii::Semaphore(context.device, semaphoreCreateInfo);

            vk::FenceCreateInfo fenceCreateInfo{
                .flags = vk::FenceCreateFlagBits::eSignaled,
            };
            inFlight = vk::raii::Fence(context.device, fenceCreateInfo);

            return true;
        } catch (const std::exception& exception) {
            std::cerr << "[Vulkan] frame data init failed: " << exception.what() << '\n';
            shutdown();
            return false;
        }
    }

    auto FrameData::shutdown() noexcept -> void
    {
        commandBuffers = vk::raii::CommandBuffers{nullptr};
        imageAvailable = vk::raii::Semaphore{nullptr};
        renderFinished = vk::raii::Semaphore{nullptr};
        inFlight = vk::raii::Fence{nullptr};
        commandPool.clear();
    }

    auto FrameData::commandBuffer() -> vk::raii::CommandBuffer&
    {
        return commandBuffers[0];
    }

    auto FrameData::commandBuffer() const -> const vk::raii::CommandBuffer&
    {
        return commandBuffers[0];
    }

    auto FrameData::beginRecording() -> bool
    {
        try
        {
            commandPool.reset();

            vk::CommandBufferBeginInfo beginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            };
            commandBuffer().begin(beginInfo);
            return true;
        } catch (const std::exception& exception) {
            std::cerr << "[Vulkan] beginRecording failed: " << exception.what() << '\n';
            return false;
        }
    }

    auto FrameData::endRecording() -> bool
    {
        try
        {
            commandBuffer().end();
            return true;
        } catch (const std::exception& exception) {
            std::cerr << "[Vulkan] endRecording failed: " << exception.what() << '\n';
            return false;
        }
    }

}  // namespace Aerkanis
