#pragma once

#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"

namespace Aerkanis::Vk
{

    struct BufferConfig
    {
        vk::DeviceSize size{};
        vk::BufferUsageFlags usage{};
        vk::MemoryPropertyFlags memoryProperties{};
        vk::SharingMode sharingMode{vk::SharingMode::eExclusive};
        std::vector<uint32_t> queueFamilyIndices{};
    };

    struct Buffer
    {
        vk::raii::Buffer buffer{nullptr};
        vk::raii::DeviceMemory memory{nullptr};
        vk::raii::Device const* device{};
        vk::DeviceSize size{};
        vk::DeviceSize allocationSize{};
        vk::DeviceSize nonCoherentAtomSize{1};
        vk::BufferUsageFlags usage{};
        vk::MemoryPropertyFlags memoryProperties{};
        void* mappedData{};
        vk::DeviceSize mappedOffset{};
        vk::DeviceSize mappedSize{};

        auto init(Context& context, BufferConfig const& config) -> void;
        auto shutdown() noexcept -> void;
        auto map(vk::DeviceSize offset = 0, vk::DeviceSize mapSize = VK_WHOLE_SIZE) -> void*;
        auto unmap() noexcept -> void;
        auto flush(vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE) const -> void;
        auto invalidate(vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE) const -> void;
        auto write(std::span<const std::byte> data, vk::DeviceSize offset = 0) -> void;
        auto descriptorInfo(
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = VK_WHOLE_SIZE) const noexcept -> vk::DescriptorBufferInfo;
        auto handle() const noexcept -> vk::Buffer;

        template <typename T>
        auto writeObject(T const& value, vk::DeviceSize offset = 0) -> void
        {
            static_assert(std::is_trivially_copyable_v<T>);
            write(std::as_bytes(std::span<const T>{&value, 1}), offset);
        }

        template <typename T>
        auto writeSpan(std::span<const T> values, vk::DeviceSize offset = 0) -> void
        {
            static_assert(std::is_trivially_copyable_v<T>);
            write(std::as_bytes(values), offset);
        }
    };

}  // namespace Aerkanis::Vk
