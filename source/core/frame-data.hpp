#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"

namespace Aerkanis
{

    struct FrameData
    {
        vk::raii::CommandPool commandPool{nullptr};
        vk::raii::CommandBuffers commandBuffers{nullptr};
        vk::raii::Semaphore imageAvailable{nullptr};
        vk::raii::Semaphore renderFinished{nullptr};
        vk::raii::Fence inFlight{nullptr};

        auto init(Context& context) -> bool;
        auto shutdown() noexcept -> void;
        auto commandBuffer() -> vk::raii::CommandBuffer&;
        auto commandBuffer() const -> const vk::raii::CommandBuffer&;
        auto beginRecording() -> bool;
        auto endRecording() -> bool;
    };

}  // namespace Aerkanis
