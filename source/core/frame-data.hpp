#pragma once

#include <cstddef>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"

namespace Aerkanis
{

    inline constexpr std::size_t DefaultFramesInFlight = 2;

    struct FrameData
    {
        vk::raii::CommandPool commandPool{nullptr};
        vk::raii::CommandBuffer commandBuffer{nullptr};
        vk::raii::Semaphore imageAvailable{nullptr};
        vk::raii::Semaphore renderFinished{nullptr};
        vk::raii::Fence inFlight{nullptr};

        auto init(Context& context, uint32_t queueFamilyIndex) -> bool;
        auto shutdown() noexcept -> void;
        auto begin() -> bool;
        auto end() -> bool;
    };

    struct FrameSet
    {
        std::vector<FrameData> frames{};
        std::size_t currentFrame{0};

        auto init(Context& context, std::size_t frameCount = DefaultFramesInFlight) -> bool;
        auto shutdown() noexcept -> void;
        auto frame() -> FrameData&;
        auto frame() const -> const FrameData&;
        auto advance() noexcept -> void;
        auto size() const noexcept -> std::size_t;
    };

}  // namespace Aerkanis
