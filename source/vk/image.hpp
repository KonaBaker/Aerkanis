#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"

namespace Aerkanis::Vk
{

    struct ImageViewConfig
    {
        vk::Format format{vk::Format::eUndefined};
        vk::ImageViewType viewType{vk::ImageViewType::e2D};
        vk::ImageAspectFlags aspectMask{vk::ImageAspectFlagBits::eColor};
        uint32_t baseMipLevel{};
        uint32_t levelCount{1};
        uint32_t baseArrayLayer{};
        uint32_t layerCount{1};
        vk::ComponentMapping components{};
    };

    struct ImageConfig
    {
        vk::ImageType imageType{vk::ImageType::e2D};
        vk::Format format{vk::Format::eUndefined};
        vk::Extent3D extent{.width = 1, .height = 1, .depth = 1};
        uint32_t mipLevels{1};
        uint32_t arrayLayers{1};
        vk::SampleCountFlagBits samples{vk::SampleCountFlagBits::e1};
        vk::ImageTiling tiling{vk::ImageTiling::eOptimal};
        vk::ImageUsageFlags usage{};
        vk::MemoryPropertyFlags memoryProperties{vk::MemoryPropertyFlagBits::eDeviceLocal};
        vk::SharingMode sharingMode{vk::SharingMode::eExclusive};
        vk::ImageLayout initialLayout{vk::ImageLayout::eUndefined};
        std::vector<uint32_t> queueFamilyIndices{};
        bool createView{true};
        ImageViewConfig view{};
    };

    auto createImageView(
        vk::raii::Device const& device,
        vk::Image image,
        ImageViewConfig config) -> vk::raii::ImageView;

    struct Image
    {
        vk::raii::Image image{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
        vk::raii::ImageView view{nullptr};
        vk::Format format{vk::Format::eUndefined};
        vk::Extent3D extent{};
        uint32_t mipLevels{};
        uint32_t arrayLayers{};
        vk::ImageUsageFlags usage{};
        vk::MemoryPropertyFlags memoryProperties{};

        auto init(Context& context, ImageConfig config) -> void;
        auto shutdown() noexcept -> void;
        auto createView(vk::raii::Device const& device, ImageViewConfig config) -> void;
        auto descriptorInfo(
            vk::ImageLayout layout,
            vk::Sampler sampler = vk::Sampler{}) const noexcept -> vk::DescriptorImageInfo;
        auto subresourceRange(vk::ImageAspectFlags aspectMask) const noexcept -> vk::ImageSubresourceRange;
        auto handle() const noexcept -> vk::Image;
        auto viewHandle() const noexcept -> vk::ImageView;
    };

}  // namespace Aerkanis::Vk
