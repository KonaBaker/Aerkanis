#include "vk/buffer.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "vk/memory.hpp"

namespace Aerkanis::Vk
{

    namespace
    {

        auto validateBufferConfig(BufferConfig const& config) -> void
        {
            if (config.size == 0)
            {
                throw std::runtime_error("Buffer requires a non-zero size");
            }

            if (!static_cast<bool>(config.usage))
            {
                throw std::runtime_error("Buffer requires at least one usage flag");
            }
        }

        auto validateWriteRange(Buffer const& buffer, vk::DeviceSize offset, vk::DeviceSize writeSize) -> void
        {
            if (writeSize == 0)
            {
                return;
            }

            if (offset > buffer.size || writeSize > buffer.size - offset)
            {
                throw std::runtime_error("Buffer write range exceeds allocation size");
            }
        }

        auto validateMapRange(Buffer const& buffer, vk::DeviceSize offset, vk::DeviceSize mapSize) -> void
        {
            if (!static_cast<bool>(buffer.memoryProperties & vk::MemoryPropertyFlagBits::eHostVisible))
            {
                throw std::runtime_error("Buffer mapping requires host-visible memory");
            }

            if (offset > buffer.size)
            {
                throw std::runtime_error("Buffer map offset exceeds buffer size");
            }

            if (mapSize != VK_WHOLE_SIZE && mapSize > buffer.size - offset)
            {
                throw std::runtime_error("Buffer map range exceeds buffer size");
            }
        }

        auto alignDown(vk::DeviceSize value, vk::DeviceSize alignment) noexcept -> vk::DeviceSize
        {
            if (alignment <= 1)
            {
                return value;
            }

            return value - (value % alignment);
        }

        auto alignUp(vk::DeviceSize value, vk::DeviceSize alignment) noexcept -> vk::DeviceSize
        {
            if (alignment <= 1)
            {
                return value;
            }

            const vk::DeviceSize remainder = value % alignment;
            return remainder == 0 ? value : value + alignment - remainder;
        }

        auto mappedMemoryRange(Buffer const& buffer, vk::DeviceSize offset, vk::DeviceSize range)
            -> vk::MappedMemoryRange
        {
            if (static_cast<vk::DeviceMemory>(*buffer.memory) == VK_NULL_HANDLE)
            {
                throw std::runtime_error("Buffer mapped memory range requires allocated memory");
            }

            if (offset > buffer.allocationSize)
            {
                throw std::runtime_error("Buffer mapped memory offset exceeds allocation size");
            }

            const vk::DeviceSize availableSize = buffer.allocationSize - offset;
            const vk::DeviceSize requestedSize = range == VK_WHOLE_SIZE ? availableSize : range;
            if (requestedSize > availableSize)
            {
                throw std::runtime_error("Buffer mapped memory range exceeds allocation size");
            }

            const vk::DeviceSize atomSize = std::max<vk::DeviceSize>(1, buffer.nonCoherentAtomSize);
            const vk::DeviceSize alignedOffset = alignDown(offset, atomSize);
            const vk::DeviceSize requestedEnd = offset + requestedSize;
            const vk::DeviceSize alignedEnd =
                std::min(buffer.allocationSize, alignUp(requestedEnd, atomSize));

            return vk::MappedMemoryRange{
                .memory = static_cast<vk::DeviceMemory>(*buffer.memory),
                .offset = alignedOffset,
                .size = alignedEnd == buffer.allocationSize ? VK_WHOLE_SIZE : alignedEnd - alignedOffset,
            };
        }

        auto mappedRangeContains(
            Buffer const& buffer,
            vk::DeviceSize offset,
            vk::DeviceSize writeSize) -> bool
        {
            if (buffer.mappedData == nullptr)
            {
                return false;
            }

            if (offset < buffer.mappedOffset)
            {
                return false;
            }

            if (buffer.mappedSize == VK_WHOLE_SIZE)
            {
                return true;
            }

            const vk::DeviceSize mappedEnd = buffer.mappedOffset + buffer.mappedSize;
            if (offset > mappedEnd)
            {
                return false;
            }
            return writeSize <= mappedEnd - offset;
        }

    }  // namespace

    auto Buffer::init(Context& context, BufferConfig const& config) -> void
    {
        shutdown();
        validateBufferConfig(config);

        const bool concurrentSharing =
            config.sharingMode == vk::SharingMode::eConcurrent &&
            !config.queueFamilyIndices.empty();

        vk::BufferCreateInfo bufferCreateInfo{
            .size = config.size,
            .usage = config.usage,
            .sharingMode = concurrentSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
            .queueFamilyIndexCount =
                concurrentSharing ? static_cast<uint32_t>(config.queueFamilyIndices.size()) : 0,
            .pQueueFamilyIndices =
                concurrentSharing ? config.queueFamilyIndices.data() : nullptr,
        };

        buffer = vk::raii::Buffer(context.device, bufferCreateInfo);

        const vk::MemoryRequirements memoryRequirements = buffer.getMemoryRequirements();
        vk::MemoryAllocateInfo memoryAllocateInfo{
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex =
                findMemoryType(context, memoryRequirements.memoryTypeBits, config.memoryProperties),
        };

        memory = vk::raii::DeviceMemory(context.device, memoryAllocateInfo);
        buffer.bindMemory(static_cast<vk::DeviceMemory>(*memory), 0);

        device = &context.device;
        size = config.size;
        allocationSize = memoryRequirements.size;
        nonCoherentAtomSize = context.physicalDevice.getProperties().limits.nonCoherentAtomSize;
        usage = config.usage;
        memoryProperties = config.memoryProperties;
    }

    auto Buffer::shutdown() noexcept -> void
    {
        unmap();
        buffer.clear();
        memory.clear();
        device = nullptr;
        size = 0;
        allocationSize = 0;
        nonCoherentAtomSize = 1;
        usage = {};
        memoryProperties = {};
        mappedOffset = 0;
        mappedSize = 0;
    }

    auto Buffer::map(vk::DeviceSize offset, vk::DeviceSize mapSize) -> void*
    {
        if (static_cast<vk::DeviceMemory>(*memory) == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Buffer::map requires allocated memory");
        }

        validateMapRange(*this, offset, mapSize);

        if (mappedData != nullptr)
        {
            return mappedData;
        }

        mappedData = memory.mapMemory(offset, mapSize);
        mappedOffset = offset;
        mappedSize = mapSize;
        return mappedData;
    }

    auto Buffer::unmap() noexcept -> void
    {
        if (mappedData == nullptr)
        {
            return;
        }

        memory.unmapMemory();
        mappedData = nullptr;
        mappedOffset = 0;
        mappedSize = 0;
    }

    auto Buffer::flush(vk::DeviceSize offset, vk::DeviceSize range) const -> void
    {
        if (static_cast<bool>(memoryProperties & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            return;
        }

        if (device == nullptr)
        {
            throw std::runtime_error("Buffer::flush requires a valid owner device");
        }

        (*device).flushMappedMemoryRanges(mappedMemoryRange(*this, offset, range));
    }

    auto Buffer::invalidate(vk::DeviceSize offset, vk::DeviceSize range) const -> void
    {
        if (static_cast<bool>(memoryProperties & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            return;
        }

        if (device == nullptr)
        {
            throw std::runtime_error("Buffer::invalidate requires a valid owner device");
        }

        (*device).invalidateMappedMemoryRanges(mappedMemoryRange(*this, offset, range));
    }

    auto Buffer::write(std::span<const std::byte> data, vk::DeviceSize offset) -> void
    {
        const vk::DeviceSize writeSize = static_cast<vk::DeviceSize>(data.size_bytes());
        validateWriteRange(*this, offset, writeSize);

        if (writeSize == 0)
        {
            return;
        }

        if (mappedRangeContains(*this, offset, writeSize))
        {
            auto* destination = static_cast<std::byte*>(mappedData) + (offset - mappedOffset);
            std::memcpy(destination, data.data(), data.size_bytes());
            flush(offset, writeSize);
            return;
        }

        if (mappedData != nullptr)
        {
            throw std::runtime_error("Buffer write range is outside the currently mapped range");
        }

        void* transientMap = memory.mapMemory(offset, writeSize);
        std::memcpy(transientMap, data.data(), data.size_bytes());
        flush(offset, writeSize);
        memory.unmapMemory();
    }

    auto Buffer::descriptorInfo(vk::DeviceSize offset, vk::DeviceSize range) const noexcept
        -> vk::DescriptorBufferInfo
    {
        return vk::DescriptorBufferInfo{
            .buffer = handle(),
            .offset = offset,
            .range = range,
        };
    }

    auto Buffer::handle() const noexcept -> vk::Buffer
    {
        return static_cast<vk::Buffer>(*buffer);
    }

}  // namespace Aerkanis::Vk
