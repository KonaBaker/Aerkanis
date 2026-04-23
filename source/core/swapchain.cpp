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

    auto chooseSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& formats) -> vk::SurfaceFormatKHR
    {
        for (const vk::SurfaceFormatKHR& format : formats)
        {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return format;
            }
        }
        return formats.front();
    }

    auto choosePresentMode(std::vector<vk::PresentModeKHR> const& modes) -> vk::PresentModeKHR
    {
        for (const vk::PresentModeKHR mode : modes)
        {
            if (mode == vk::PresentModeKHR::eMailbox)
            {
                return mode;
            }
        }
        for (const vk::PresentModeKHR mode : modes)
        {
            if (mode == vk::PresentModeKHR::eImmediate)
            {
                return mode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    auto clampExtent(vk::Extent2D requested, vk::SurfaceCapabilitiesKHR const& capabilities) -> vk::Extent2D
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        requested.width = std::max(requested.width, 1u);
        requested.height = std::max(requested.height, 1u);
        requested.width = std::clamp(requested.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        requested.height = std::clamp(requested.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return requested;
    }

    auto createImageView(const vk::raii::Device& device, vk::Image image, vk::Format format) -> vk::raii::ImageView
    {
        vk::ImageViewCreateInfo imageViewCreateInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .components = vk::ComponentMapping{
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity,
            },
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

auto Swapchain::init(Context& context, const vk::raii::SurfaceKHR& surface, vk::Extent2D requestedExtent) -> bool
{
    shutdown();

    try
    {
        const vk::SurfaceCapabilitiesKHR surfaceCapabilities =
            context.physicalDevice.getSurfaceCapabilitiesKHR(static_cast<vk::SurfaceKHR>(*surface));
        const std::vector<vk::SurfaceFormatKHR> surfaceFormats =
            context.physicalDevice.getSurfaceFormatsKHR(static_cast<vk::SurfaceKHR>(*surface));
        const std::vector<vk::PresentModeKHR> surfacePresentModes =
            context.physicalDevice.getSurfacePresentModesKHR(static_cast<vk::SurfaceKHR>(*surface));

        if (surfaceFormats.empty() || surfacePresentModes.empty())
        {
            std::cerr << "[Vulkan] surface does not expose formats or present modes\n";
            return false;
        }

        const vk::SurfaceFormatKHR chosenFormat = Details::chooseSurfaceFormat(surfaceFormats);
        imageFormat = chosenFormat.format;
        presentMode = Details::choosePresentMode(surfacePresentModes);
        extent = Details::clampExtent(requestedExtent, surfaceCapabilities);

        uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount > 0)
        {
            imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
        }

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
        imageViews.reserve(images.size());
        for (vk::Image image : images)
        {
            imageViews.emplace_back(Details::createImageView(context.device, image, imageFormat));
        }

        recreateRequired = false;
        return true;
    } catch (const std::exception& exception) {
        std::cerr << "[Vulkan] swapchain init failed: " << exception.what() << '\n';
        shutdown();
        return false;
    }
}

auto Swapchain::shutdown() noexcept -> void
{
    imageViews.clear();
    images.clear();
    swapchain.clear();
    imageFormat = vk::Format::eUndefined;
    extent = vk::Extent2D{.width = 0, .height = 0};
    presentMode = vk::PresentModeKHR::eFifo;
    recreateRequired = false;
}

auto Swapchain::recreate(Context& context, const vk::raii::SurfaceKHR& surface, vk::Extent2D requestedExtent) -> bool
{
    return init(context, surface, requestedExtent);
}

auto Swapchain::acquireNextImage(const vk::raii::Semaphore& imageAvailable, const vk::raii::Fence& inFlight) -> std::optional<uint32_t>
{
    if (static_cast<vk::SwapchainKHR>(*swapchain) == VK_NULL_HANDLE)
    {
        return std::nullopt;
    }

    try
    {
        const vk::ResultValue<uint32_t> result = swapchain.acquireNextImage(
            std::numeric_limits<uint64_t>::max(),
            static_cast<vk::Semaphore>(imageAvailable),
            static_cast<vk::Fence>(inFlight));

        if (result.result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateRequired = true;
            return std::nullopt;
        }

        if (result.result == vk::Result::eSuboptimalKHR)
        {
            recreateRequired = true;
        }

        return result.value;
    } catch (const std::exception& exception) {
        std::cerr << "[Vulkan] acquireNextImage failed: " << exception.what() << '\n';
        recreateRequired = true;
        return std::nullopt;
    }
}

auto Swapchain::present(const Context& context, uint32_t imageIndex, const vk::raii::Semaphore& renderFinished) -> vk::Result
{
    if (static_cast<vk::SwapchainKHR>(*swapchain) == VK_NULL_HANDLE)
    {
        return vk::Result::eErrorOutOfDateKHR;
    }

    const vk::Semaphore waitSemaphore = static_cast<vk::Semaphore>(renderFinished);
    const vk::SwapchainKHR swapchainHandle = static_cast<vk::SwapchainKHR>(*swapchain);
    const vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &waitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchainHandle,
        .pImageIndices = &imageIndex,
    };

    try
    {
        const vk::Result result = context.presentQueue.presentKHR(presentInfo);
        if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateRequired = true;
        }
        return result;
    } catch (const std::exception& exception) {
        std::cerr << "[Vulkan] present failed: " << exception.what() << '\n';
        recreateRequired = true;
        return vk::Result::eErrorOutOfDateKHR;
    }
}

}  // namespace Aerkanis
