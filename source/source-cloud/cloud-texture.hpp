#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"
#include "vk/image.hpp"

namespace Aerkanis::Cloud
{

    struct CpuImage
    {
        int width{};
        int height{};
        int channels{4};
        std::vector<std::byte> pixels{};
    };

    struct TextureResource
    {
        Vk::Image image{};
        vk::raii::Sampler sampler{nullptr};
        vk::ImageLayout layout{vk::ImageLayout::eUndefined};

        auto shutdown() noexcept -> void;
        auto descriptorInfo() const noexcept -> vk::DescriptorImageInfo;
        auto ready() const noexcept -> bool;
    };

    struct CloudTextureUploader
    {
        Context* context{};
        vk::raii::CommandPool commandPool{nullptr};

        auto init(Context& nextContext) -> void;
        auto shutdown() noexcept -> void;
        auto uploadTexture2D(
            std::filesystem::path const& path,
            vk::Format format = vk::Format::eR8G8B8A8Unorm) -> TextureResource;
        auto uploadVolumeSlices(
            std::filesystem::path const& directory,
            std::string const& prefix,
            std::string const& suffix,
            uint32_t sliceCount,
            vk::Format format = vk::Format::eR8G8B8A8Unorm) -> TextureResource;

    private:
        auto beginSingleUseCommands() -> vk::raii::CommandBuffer;
        auto endSingleUseCommands(vk::raii::CommandBuffer& commandBuffer) -> void;
        auto uploadPixels(
            std::vector<std::byte> const& pixels,
            uint32_t width,
            uint32_t height,
            uint32_t depth,
            vk::ImageType imageType,
            vk::ImageViewType viewType,
            vk::Format format) -> TextureResource;
    };

    auto findCloudAssetPath(std::filesystem::path const& relativePath) -> std::filesystem::path;
    auto loadCpuImage(std::filesystem::path const& path) -> CpuImage;

}  // namespace Aerkanis::Cloud
