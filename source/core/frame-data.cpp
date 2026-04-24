#include "core/frame-data.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace Aerkanis
{

    auto FrameData::init(Context& context, uint32_t queueFamilyIndex) -> bool
    {
        shutdown();

        if (static_cast<vk::Device>(*context.device) == VK_NULL_HANDLE)
        {
            std::cerr << "[Vulkan] frame data requires a valid logical device\n";
            return false;
        }

        try
        {
            vk::CommandPoolCreateInfo commandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queueFamilyIndex,
            };

            commandPool = vk::raii::CommandPool(context.device, commandPoolCreateInfo);

            vk::CommandBufferAllocateInfo commandBufferAllocateInfo{
                .commandPool = static_cast<vk::CommandPool>(*commandPool),
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            std::vector<vk::raii::CommandBuffer> allocatedCommandBuffers =
                context.device.allocateCommandBuffers(commandBufferAllocateInfo);
            if (allocatedCommandBuffers.empty())
            {
                std::cerr << "[Vulkan] frame data failed to allocate a command buffer\n";
                shutdown();
                return false;
            }

            commandBuffer = std::move(allocatedCommandBuffers.front());

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
        commandBuffer = vk::raii::CommandBuffer{nullptr};
        imageAvailable = vk::raii::Semaphore{nullptr};
        renderFinished = vk::raii::Semaphore{nullptr};
        inFlight = vk::raii::Fence{nullptr};
        commandPool.clear();
    }

    auto FrameData::begin() -> bool
    {
        try
        {
            commandPool.reset();

            vk::CommandBufferBeginInfo beginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            };
            commandBuffer.begin(beginInfo);
            return true;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Vulkan] frame data begin failed: " << exception.what() << '\n';
            return false;
        }
    }

    auto FrameData::end() -> bool
    {
        try
        {
            commandBuffer.end();
            return true;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Vulkan] frame data end failed: " << exception.what() << '\n';
            return false;
        }
    }

    auto FrameSet::init(Context& context, std::size_t frameCount) -> bool
    {
        shutdown();

        if (frameCount == 0)
        {
            std::cerr << "[Vulkan] frame set requires at least one frame\n";
            return false;
        }

        if (!context.queueFamilies.graphicsFamily.has_value())
        {
            std::cerr << "[Vulkan] frame set requires a graphics queue family\n";
            return false;
        }

        frames.resize(frameCount);
        for (FrameData& frame : frames)
        {
            if (!frame.init(context, context.queueFamilies.graphicsFamily.value()))
            {
                shutdown();
                return false;
            }
        }

        currentFrame = 0;
        return true;
    }

    auto FrameSet::shutdown() noexcept -> void
    {
        for (FrameData& frame : frames)
        {
            frame.shutdown();
        }

        frames.clear();
        currentFrame = 0;
    }

    auto FrameSet::frame() -> FrameData&
    {
        if (frames.empty())
        {
            throw std::runtime_error("FrameSet::frame called on an empty frame set");
        }
        return frames[currentFrame];
    }

    auto FrameSet::frame() const -> const FrameData&
    {
        if (frames.empty())
        {
            throw std::runtime_error("FrameSet::frame called on an empty frame set");
        }
        return frames[currentFrame];
    }

    auto FrameSet::advance() noexcept -> void
    {
        if (!frames.empty())
        {
            currentFrame = (currentFrame + 1) % frames.size();
        }
    }

    auto FrameSet::size() const noexcept -> std::size_t
    {
        return frames.size();
    }

}  // namespace Aerkanis
