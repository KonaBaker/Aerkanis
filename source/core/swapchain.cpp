#include "core/swapchain.hpp"

#include <array>
#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

namespace Aerkanis::Details
{

namespace
{

    auto chooseSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats) -> vk::SurfaceFormatKHR
    {
        const auto iter = std::ranges::find_if(availableFormats, [](vk::SurfaceFormatKHR const& surfaceFormat){
            return surfaceFormat.format == vk::Format::eB8G8R8A8Srgb && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        if(iter == availableFormats.end()) {
            throw std::runtime_error("has no available surface format!");
        }
        return *iter;
    }

    auto choosePresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes) -> vk::PresentModeKHR
    {
        return std::ranges::any_of(availablePresentModes, [](const auto& presentMode){
            return presentMode == vk::PresentModeKHR::eMailbox;
        }) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    auto chooseExtent(vk::SurfaceCapabilitiesKHR const& capabilities, Window const& window) -> vk::Extent2D
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        int width, height;
        vk::Extent2D extent;
        window.getFramebufferSize(width, height);

        extent.height = static_cast<uint32_t>(height);
        extent.width = static_cast<uint32_t>(width);

        return extent;
    }

    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &capabilities)
    {
        auto minImageCount = std::max(3u, capabilities.minImageCount);
        if ((0 < capabilities.maxImageCount) && (capabilities.maxImageCount < minImageCount))
        {
            minImageCount = capabilities.maxImageCount;
        }
        return minImageCount;
    }


    auto createImageView(const vk::raii::Device& device, vk::Image image, vk::Format format) -> vk::raii::ImageView
    {
        vk::ImageViewCreateInfo imageViewCreateInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        return vk::raii::ImageView(device, imageViewCreateInfo);
    }

}  // namespace

}  // namespace Aerkanis::Details

namespace Aerkanis
{

auto Swapchain::init(Context& context, const vk::raii::SurfaceKHR& surface, Window const& window) -> void
{
    const vk::SurfaceCapabilitiesKHR surfaceCapabilities =
        context.physicalDevice.getSurfaceCapabilitiesKHR(static_cast<vk::SurfaceKHR>(*surface));
    extent = Details::chooseExtent(surfaceCapabilities, window);
    uint32_t imageCount = Details::chooseSwapMinImageCount(surfaceCapabilities);

    const std::vector<vk::SurfaceFormatKHR> surfaceFormats =
        context.physicalDevice.getSurfaceFormatsKHR(static_cast<vk::SurfaceKHR>(*surface));
    const vk::SurfaceFormatKHR chosenFormat = Details::chooseSurfaceFormat(surfaceFormats);
    imageFormat = chosenFormat.format;

    const std::vector<vk::PresentModeKHR> surfacePresentModes =
        context.physicalDevice.getSurfacePresentModesKHR(static_cast<vk::SurfaceKHR>(*surface));
    presentMode = Details::choosePresentMode(surfacePresentModes);

    const std::array<uint32_t, 2> queueFamilyIndices{
        context.queueFamilies.graphicsFamily.value_or(0),
        context.queueFamilies.presentFamily.value_or(0),
    };

    const bool useConcurrentSharing =
        context.queueFamilies.graphicsFamily.has_value() &&
        context.queueFamilies.presentFamily.has_value() &&
        context.queueFamilies.graphicsFamily.value() != context.queueFamilies.presentFamily.value();

    vk::SwapchainCreateInfoKHR swapchainCreateInfo{
        .surface = static_cast<vk::SurfaceKHR>(*surface),
        .minImageCount = imageCount,
        .imageFormat = imageFormat,
        .imageColorSpace = chosenFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
        .imageSharingMode = useConcurrentSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = useConcurrentSharing ? static_cast<uint32_t>(queueFamilyIndices.size()) : 0,
        .pQueueFamilyIndices = useConcurrentSharing ? queueFamilyIndices.data() : nullptr,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = nullptr,
    };

    swapchain = vk::raii::SwapchainKHR(context.device, swapchainCreateInfo);
    images = swapchain.getImages();

    imageViews.clear();
    imageViews.reserve(images.size());
    for (const auto& image : images) {
        imageViews.emplace_back(Details::createImageView(context.device, image, imageFormat));
    }
}

auto Swapchain::cleanup() noexcept -> void
{
    imageViews.clear();
    images.clear();
    swapchain.clear();
    imageFormat = vk::Format::eUndefined;
    extent = vk::Extent2D{.width = 0, .height = 0};
    presentMode = vk::PresentModeKHR::eFifo;
}

auto Swapchain::recreate(Context& context, const vk::raii::SurfaceKHR& surface, const Window& window) -> void
{
    cleanup();
    init(context, surface, window);
}

}  // namespace Aerkanis
