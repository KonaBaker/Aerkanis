#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "vk/buffer.hpp"
#include "vk/image.hpp"

namespace Aerkanis::Vk
{

    struct BufferDescriptorWrite
    {
        vk::DescriptorSet destinationSet{};
        uint32_t binding{};
        uint32_t arrayElement{};
        vk::DescriptorType descriptorType{};
        std::vector<vk::DescriptorBufferInfo> bufferInfos{};
    };

    struct ImageDescriptorWrite
    {
        vk::DescriptorSet destinationSet{};
        uint32_t binding{};
        uint32_t arrayElement{};
        vk::DescriptorType descriptorType{};
        std::vector<vk::DescriptorImageInfo> imageInfos{};
    };

    struct DescriptorWriter
    {
        std::vector<BufferDescriptorWrite> bufferWrites{};
        std::vector<ImageDescriptorWrite> imageWrites{};
        std::vector<vk::CopyDescriptorSet> copies{};

        auto clear() noexcept -> void;
        auto empty() const noexcept -> bool;
        auto writeBuffer(
            vk::DescriptorSet destinationSet,
            uint32_t binding,
            vk::DescriptorType descriptorType,
            vk::DescriptorBufferInfo bufferInfo,
            uint32_t arrayElement = 0) -> DescriptorWriter&;
        auto writeBuffers(
            vk::DescriptorSet destinationSet,
            uint32_t binding,
            vk::DescriptorType descriptorType,
            std::span<const vk::DescriptorBufferInfo> bufferInfos,
            uint32_t arrayElement = 0) -> DescriptorWriter&;
        auto writeBuffer(
            vk::DescriptorSet destinationSet,
            uint32_t binding,
            vk::DescriptorType descriptorType,
            Buffer const& buffer,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = VK_WHOLE_SIZE,
            uint32_t arrayElement = 0) -> DescriptorWriter&;
        auto writeImage(
            vk::DescriptorSet destinationSet,
            uint32_t binding,
            vk::DescriptorType descriptorType,
            vk::DescriptorImageInfo imageInfo,
            uint32_t arrayElement = 0) -> DescriptorWriter&;
        auto writeImages(
            vk::DescriptorSet destinationSet,
            uint32_t binding,
            vk::DescriptorType descriptorType,
            std::span<const vk::DescriptorImageInfo> imageInfos,
            uint32_t arrayElement = 0) -> DescriptorWriter&;
        auto writeImage(
            vk::DescriptorSet destinationSet,
            uint32_t binding,
            vk::DescriptorType descriptorType,
            Image const& image,
            vk::ImageLayout layout,
            vk::Sampler sampler = vk::Sampler{},
            uint32_t arrayElement = 0) -> DescriptorWriter&;
        auto copy(
            vk::DescriptorSet sourceSet,
            uint32_t sourceBinding,
            vk::DescriptorSet destinationSet,
            uint32_t destinationBinding,
            uint32_t descriptorCount = 1,
            uint32_t sourceArrayElement = 0,
            uint32_t destinationArrayElement = 0) -> DescriptorWriter&;
        auto update(vk::raii::Device const& device) const -> void;
    };

}  // namespace Aerkanis::Vk
