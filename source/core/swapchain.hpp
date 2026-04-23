#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"

namespace Aerkanis
{

struct Swapchain
{
    vk::raii::SwapchainKHR swapchain{nullptr};
    std::vector<vk::Image> images{};
    std::vector<vk::raii::ImageView> imageViews{};
    vk::Format imageFormat{vk::Format::eUndefined};
    vk::Extent2D extent{};
    vk::PresentModeKHR presentMode{vk::PresentModeKHR::eFifo};
    bool recreateRequired{false};

    auto init(Context& context, const vk::raii::SurfaceKHR& surface, vk::Extent2D requestedExtent) -> bool;
    auto shutdown() noexcept -> void;
    auto recreate(Context& context, const vk::raii::SurfaceKHR& surface, vk::Extent2D requestedExtent) -> bool;
    auto acquireNextImage(const vk::raii::Semaphore& imageAvailable, const vk::raii::Fence& inFlight) -> std::optional<uint32_t>;
    auto present(const Context& context, uint32_t imageIndex, const vk::raii::Semaphore& renderFinished) -> vk::Result;
};

}  // namespace Aerkanis
