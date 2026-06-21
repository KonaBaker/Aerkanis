#include "source-cloud/cloud-texture.hpp"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "vk/buffer.hpp"

namespace Aerkanis::Cloud
{

    namespace
    {

        auto colorRange(uint32_t layerCount = 1) noexcept -> vk::ImageSubresourceRange
        {
            return vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = layerCount,
            };
        }

        auto transitionImage(
            vk::raii::CommandBuffer const& commandBuffer,
            vk::Image image,
            vk::ImageLayout oldLayout,
            vk::ImageLayout newLayout,
            vk::AccessFlags sourceAccess,
            vk::AccessFlags destinationAccess,
            vk::PipelineStageFlags sourceStage,
            vk::PipelineStageFlags destinationStage,
            vk::ImageSubresourceRange range) -> void
        {
            const vk::ImageMemoryBarrier barrier{
                .srcAccessMask = sourceAccess,
                .dstAccessMask = destinationAccess,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = range,
            };

            commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
        }

        auto makeSampler(vk::raii::Device const& device) -> vk::raii::Sampler
        {
            vk::SamplerCreateInfo samplerCreateInfo{
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = vk::SamplerAddressMode::eRepeat,
                .addressModeV = vk::SamplerAddressMode::eRepeat,
                .addressModeW = vk::SamplerAddressMode::eRepeat,
                .mipLodBias = 0.0F,
                .anisotropyEnable = VK_FALSE,
                .maxAnisotropy = 1.0F,
                .compareEnable = VK_FALSE,
                .minLod = 0.0F,
                .maxLod = 0.0F,
                .borderColor = vk::BorderColor::eFloatOpaqueBlack,
                .unnormalizedCoordinates = VK_FALSE,
            };

            return vk::raii::Sampler(device, samplerCreateInfo);
        }

        auto ensureImageLoaded(CpuImage const& image, std::filesystem::path const& path) -> void
        {
            if (image.width <= 0 || image.height <= 0 || image.pixels.empty())
            {
                throw std::runtime_error("failed to load cloud image: " + path.string());
            }
        }

        auto numberedPath(
            std::filesystem::path const& directory,
            std::string const& prefix,
            uint32_t index,
            std::string const& suffix) -> std::filesystem::path
        {
            return directory / (prefix + std::to_string(index) + suffix);
        }

    }  // namespace

    auto TextureResource::shutdown() noexcept -> void
    {
        sampler.clear();
        image.shutdown();
        layout = vk::ImageLayout::eUndefined;
    }

    auto TextureResource::descriptorInfo() const noexcept -> vk::DescriptorImageInfo
    {
        return vk::DescriptorImageInfo{
            .sampler = static_cast<vk::Sampler>(*sampler),
            .imageView = image.viewHandle(),
            .imageLayout = layout,
        };
    }

    auto TextureResource::ready() const noexcept -> bool
    {
        return static_cast<vk::Image>(*image.image) != VK_NULL_HANDLE &&
               static_cast<vk::ImageView>(*image.view) != VK_NULL_HANDLE &&
               static_cast<vk::Sampler>(*sampler) != VK_NULL_HANDLE;
    }

    auto findCloudAssetPath(std::filesystem::path const& relativePath) -> std::filesystem::path
    {
        const std::array candidates{
            std::filesystem::current_path() / relativePath,
            std::filesystem::current_path() / "asset" / relativePath,
            std::filesystem::current_path() / ".." / "asset" / relativePath,
            std::filesystem::current_path() / "build" / "asset" / relativePath,
        };

        for (std::filesystem::path const& candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                return candidate;
            }
        }

        return candidates.front();
    }

    auto loadCpuImage(std::filesystem::path const& path) -> CpuImage
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* rawPixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        if (rawPixels == nullptr)
        {
            throw std::runtime_error(
                "stb_image failed to load " + path.string() + ": " + stbi_failure_reason());
        }

        CpuImage image{
            .width = width,
            .height = height,
            .channels = 4,
            .pixels = std::vector<std::byte>(static_cast<std::size_t>(width) * height * 4),
        };
        std::memcpy(image.pixels.data(), rawPixels, image.pixels.size());
        stbi_image_free(rawPixels);
        return image;
    }

    auto CloudTextureUploader::init(Context& nextContext) -> void
    {
        shutdown();

        if (!nextContext.queueFamilies.graphicsFamily.has_value())
        {
            throw std::runtime_error("CloudTextureUploader requires a graphics queue family");
        }

        context = &nextContext;
        vk::CommandPoolCreateInfo commandPoolCreateInfo{
            .flags = vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = nextContext.queueFamilies.graphicsFamily.value(),
        };
        commandPool = vk::raii::CommandPool(nextContext.device, commandPoolCreateInfo);
    }

    auto CloudTextureUploader::shutdown() noexcept -> void
    {
        commandPool.clear();
        context = nullptr;
    }

    auto CloudTextureUploader::uploadTexture2D(
        std::filesystem::path const& path,
        vk::Format format) -> TextureResource
    {
        CpuImage image = loadCpuImage(path);
        ensureImageLoaded(image, path);

        return uploadPixels(
            image.pixels,
            static_cast<uint32_t>(image.width),
            static_cast<uint32_t>(image.height),
            1,
            vk::ImageType::e2D,
            vk::ImageViewType::e2D,
            format);
    }

    auto CloudTextureUploader::uploadVolumeSlices(
        std::filesystem::path const& directory,
        std::string const& prefix,
        std::string const& suffix,
        uint32_t sliceCount,
        vk::Format format) -> TextureResource
    {
        if (sliceCount == 0)
        {
            throw std::runtime_error("Cloud volume upload requires at least one slice");
        }

        CpuImage firstSlice = loadCpuImage(numberedPath(directory, prefix, 0, suffix));
        ensureImageLoaded(firstSlice, numberedPath(directory, prefix, 0, suffix));

        const uint32_t width = static_cast<uint32_t>(firstSlice.width);
        const uint32_t height = static_cast<uint32_t>(firstSlice.height);
        const std::size_t sliceSize = firstSlice.pixels.size();
        std::vector<std::byte> volumePixels(sliceSize * sliceCount);
        std::memcpy(volumePixels.data(), firstSlice.pixels.data(), sliceSize);

        for (uint32_t sliceIndex = 1; sliceIndex < sliceCount; ++sliceIndex)
        {
            const std::filesystem::path slicePath = numberedPath(directory, prefix, sliceIndex, suffix);
            CpuImage slice = loadCpuImage(slicePath);
            ensureImageLoaded(slice, slicePath);
            if (static_cast<uint32_t>(slice.width) != width || static_cast<uint32_t>(slice.height) != height)
            {
                throw std::runtime_error("cloud volume slices must share dimensions: " + slicePath.string());
            }

            std::memcpy(volumePixels.data() + sliceSize * sliceIndex, slice.pixels.data(), sliceSize);
        }

        return uploadPixels(
            volumePixels,
            width,
            height,
            sliceCount,
            vk::ImageType::e3D,
            vk::ImageViewType::e3D,
            format);
    }

    auto CloudTextureUploader::beginSingleUseCommands() -> vk::raii::CommandBuffer
    {
        if (context == nullptr || static_cast<vk::CommandPool>(*commandPool) == VK_NULL_HANDLE)
        {
            throw std::runtime_error("CloudTextureUploader is not initialized");
        }

        vk::CommandBufferAllocateInfo allocateInfo{
            .commandPool = static_cast<vk::CommandPool>(*commandPool),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };

        std::vector<vk::raii::CommandBuffer> commandBuffers =
            context->device.allocateCommandBuffers(allocateInfo);
        vk::raii::CommandBuffer commandBuffer = std::move(commandBuffers.front());
        vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        };
        commandBuffer.begin(beginInfo);
        return commandBuffer;
    }

    auto CloudTextureUploader::endSingleUseCommands(vk::raii::CommandBuffer& commandBuffer) -> void
    {
        commandBuffer.end();
        const vk::CommandBuffer rawCommandBuffer = static_cast<vk::CommandBuffer>(*commandBuffer);
        const vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &rawCommandBuffer,
        };

        context->graphicsQueue.submit(submitInfo);
        context->graphicsQueue.waitIdle();
    }

    auto CloudTextureUploader::uploadPixels(
        std::vector<std::byte> const& pixels,
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        vk::ImageType imageType,
        vk::ImageViewType viewType,
        vk::Format format) -> TextureResource
    {
        if (context == nullptr)
        {
            throw std::runtime_error("CloudTextureUploader is not initialized");
        }
        if (pixels.empty() || width == 0 || height == 0 || depth == 0)
        {
            throw std::runtime_error("Cloud texture upload requires non-empty pixels and extent");
        }

        Vk::Buffer stagingBuffer{};
        stagingBuffer.init(
            *context,
            Vk::BufferConfig{
                .size = static_cast<vk::DeviceSize>(pixels.size()),
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .memoryProperties =
                    vk::MemoryPropertyFlagBits::eHostVisible |
                    vk::MemoryPropertyFlagBits::eHostCoherent,
            });
        stagingBuffer.write(std::span<const std::byte>{pixels.data(), pixels.size()});

        TextureResource texture{};
        texture.image.init(
            *context,
            Vk::ImageConfig{
                .imageType = imageType,
                .format = format,
                .extent = vk::Extent3D{
                    .width = width,
                    .height = height,
                    .depth = depth,
                },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                .createView = true,
                .view = Vk::ImageViewConfig{
                    .format = format,
                    .viewType = viewType,
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .levelCount = 1,
                    .layerCount = 1,
                },
            });

        vk::raii::CommandBuffer commandBuffer = beginSingleUseCommands();
        transitionImage(
            commandBuffer,
            texture.image.handle(),
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            {},
            vk::AccessFlagBits::eTransferWrite,
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer,
            colorRange());

        const vk::BufferImageCopy copyRegion{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = vk::ImageSubresourceLayers{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
            .imageExtent = vk::Extent3D{
                .width = width,
                .height = height,
                .depth = depth,
            },
        };
        commandBuffer.copyBufferToImage(
            stagingBuffer.handle(),
            texture.image.handle(),
            vk::ImageLayout::eTransferDstOptimal,
            copyRegion);

        transitionImage(
            commandBuffer,
            texture.image.handle(),
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader,
            colorRange());
        endSingleUseCommands(commandBuffer);

        texture.sampler = makeSampler(context->device);
        texture.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        stagingBuffer.shutdown();
        return texture;
    }

}  // namespace Aerkanis::Cloud
