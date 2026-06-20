#include "vk/image.hpp"

#include <stdexcept>

#include "vk/memory.hpp"

namespace Aerkanis::Vk
{

    namespace
    {

        auto validateImageConfig(ImageConfig const& config) -> void
        {
            if (config.format == vk::Format::eUndefined)
            {
                throw std::runtime_error("Image requires a defined format");
            }

            if (config.extent.width == 0 || config.extent.height == 0 || config.extent.depth == 0)
            {
                throw std::runtime_error("Image requires a non-zero extent");
            }

            if (config.mipLevels == 0 || config.arrayLayers == 0)
            {
                throw std::runtime_error("Image requires at least one mip level and one array layer");
            }

            if (!static_cast<bool>(config.usage))
            {
                throw std::runtime_error("Image requires at least one usage flag");
            }
        }

        auto normalizeViewConfig(ImageConfig const& imageConfig) -> ImageViewConfig
        {
            ImageViewConfig viewConfig = imageConfig.view;
            if (viewConfig.format == vk::Format::eUndefined)
            {
                viewConfig.format = imageConfig.format;
            }
            if (viewConfig.levelCount == 0)
            {
                viewConfig.levelCount = imageConfig.mipLevels;
            }
            if (viewConfig.layerCount == 0)
            {
                viewConfig.layerCount = imageConfig.arrayLayers;
            }
            return viewConfig;
        }

    }  // namespace

    auto createImageView(
        vk::raii::Device const& device,
        vk::Image image,
        ImageViewConfig config) -> vk::raii::ImageView
    {
        if (config.format == vk::Format::eUndefined)
        {
            throw std::runtime_error("Image view requires a defined format");
        }

        if (config.levelCount == 0 || config.layerCount == 0)
        {
            throw std::runtime_error("Image view requires at least one mip level and one array layer");
        }

        vk::ImageViewCreateInfo viewCreateInfo{
            .image = image,
            .viewType = config.viewType,
            .format = config.format,
            .components = config.components,
            .subresourceRange = vk::ImageSubresourceRange{
                .aspectMask = config.aspectMask,
                .baseMipLevel = config.baseMipLevel,
                .levelCount = config.levelCount,
                .baseArrayLayer = config.baseArrayLayer,
                .layerCount = config.layerCount,
            },
        };

        return vk::raii::ImageView(device, viewCreateInfo);
    }

    auto Image::init(Context& context, ImageConfig config) -> void
    {
        shutdown();
        validateImageConfig(config);

        const bool concurrentSharing =
            config.sharingMode == vk::SharingMode::eConcurrent &&
            !config.queueFamilyIndices.empty();

        vk::ImageCreateInfo imageCreateInfo{
            .imageType = config.imageType,
            .format = config.format,
            .extent = config.extent,
            .mipLevels = config.mipLevels,
            .arrayLayers = config.arrayLayers,
            .samples = config.samples,
            .tiling = config.tiling,
            .usage = config.usage,
            .sharingMode = concurrentSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
            .queueFamilyIndexCount =
                concurrentSharing ? static_cast<uint32_t>(config.queueFamilyIndices.size()) : 0,
            .pQueueFamilyIndices =
                concurrentSharing ? config.queueFamilyIndices.data() : nullptr,
            .initialLayout = config.initialLayout,
        };

        image = vk::raii::Image(context.device, imageCreateInfo);

        const vk::MemoryRequirements memoryRequirements = image.getMemoryRequirements();
        vk::MemoryAllocateInfo memoryAllocateInfo{
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex =
                findMemoryType(context, memoryRequirements.memoryTypeBits, config.memoryProperties),
        };

        memory = vk::raii::DeviceMemory(context.device, memoryAllocateInfo);
        image.bindMemory(static_cast<vk::DeviceMemory>(*memory), 0);

        format = config.format;
        extent = config.extent;
        mipLevels = config.mipLevels;
        arrayLayers = config.arrayLayers;
        usage = config.usage;
        memoryProperties = config.memoryProperties;

        if (config.createView)
        {
            createView(context.device, normalizeViewConfig(config));
        }
    }

    auto Image::shutdown() noexcept -> void
    {
        view.clear();
        image.clear();
        memory.clear();
        format = vk::Format::eUndefined;
        extent = {};
        mipLevels = 0;
        arrayLayers = 0;
        usage = {};
        memoryProperties = {};
    }

    auto Image::createView(vk::raii::Device const& device, ImageViewConfig config) -> void
    {
        if (config.format == vk::Format::eUndefined)
        {
            config.format = format;
        }
        if (config.levelCount == 0)
        {
            config.levelCount = mipLevels;
        }
        if (config.layerCount == 0)
        {
            config.layerCount = arrayLayers;
        }

        view = Aerkanis::Vk::createImageView(device, handle(), config);
    }

    auto Image::descriptorInfo(vk::ImageLayout layout, vk::Sampler sampler) const noexcept
        -> vk::DescriptorImageInfo
    {
        return vk::DescriptorImageInfo{
            .sampler = sampler,
            .imageView = viewHandle(),
            .imageLayout = layout,
        };
    }

    auto Image::subresourceRange(vk::ImageAspectFlags aspectMask) const noexcept
        -> vk::ImageSubresourceRange
    {
        return vk::ImageSubresourceRange{
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = mipLevels,
            .baseArrayLayer = 0,
            .layerCount = arrayLayers,
        };
    }

    auto Image::handle() const noexcept -> vk::Image
    {
        return static_cast<vk::Image>(*image);
    }

    auto Image::viewHandle() const noexcept -> vk::ImageView
    {
        return static_cast<vk::ImageView>(*view);
    }

}  // namespace Aerkanis::Vk
