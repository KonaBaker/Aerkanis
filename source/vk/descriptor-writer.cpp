#include "vk/descriptor-writer.hpp"

#include <array>
#include <stdexcept>

namespace Aerkanis::Vk
{

    namespace
    {

        auto validateDescriptorSet(vk::DescriptorSet descriptorSet) -> void
        {
            if (descriptorSet == VK_NULL_HANDLE)
            {
                throw std::runtime_error("Descriptor write requires a valid destination set");
            }
        }

    }  // namespace

    auto DescriptorWriter::clear() noexcept -> void
    {
        bufferWrites.clear();
        imageWrites.clear();
        copies.clear();
    }

    auto DescriptorWriter::empty() const noexcept -> bool
    {
        return bufferWrites.empty() && imageWrites.empty() && copies.empty();
    }

    auto DescriptorWriter::writeBuffer(
        vk::DescriptorSet destinationSet,
        uint32_t binding,
        vk::DescriptorType descriptorType,
        vk::DescriptorBufferInfo bufferInfo,
        uint32_t arrayElement) -> DescriptorWriter&
    {
        const std::array infos{bufferInfo};
        return writeBuffers(destinationSet, binding, descriptorType, infos, arrayElement);
    }

    auto DescriptorWriter::writeBuffers(
        vk::DescriptorSet destinationSet,
        uint32_t binding,
        vk::DescriptorType descriptorType,
        std::span<const vk::DescriptorBufferInfo> bufferInfos,
        uint32_t arrayElement) -> DescriptorWriter&
    {
        validateDescriptorSet(destinationSet);

        if (bufferInfos.empty())
        {
            throw std::runtime_error("Descriptor buffer write requires at least one buffer info");
        }

        this->bufferWrites.push_back(BufferDescriptorWrite{
            .destinationSet = destinationSet,
            .binding = binding,
            .arrayElement = arrayElement,
            .descriptorType = descriptorType,
            .bufferInfos = std::vector<vk::DescriptorBufferInfo>{bufferInfos.begin(), bufferInfos.end()},
        });
        return *this;
    }

    auto DescriptorWriter::writeBuffer(
        vk::DescriptorSet destinationSet,
        uint32_t binding,
        vk::DescriptorType descriptorType,
        Buffer const& buffer,
        vk::DeviceSize offset,
        vk::DeviceSize range,
        uint32_t arrayElement) -> DescriptorWriter&
    {
        return writeBuffer(
            destinationSet,
            binding,
            descriptorType,
            buffer.descriptorInfo(offset, range),
            arrayElement);
    }

    auto DescriptorWriter::writeImage(
        vk::DescriptorSet destinationSet,
        uint32_t binding,
        vk::DescriptorType descriptorType,
        vk::DescriptorImageInfo imageInfo,
        uint32_t arrayElement) -> DescriptorWriter&
    {
        const std::array infos{imageInfo};
        return writeImages(destinationSet, binding, descriptorType, infos, arrayElement);
    }

    auto DescriptorWriter::writeImages(
        vk::DescriptorSet destinationSet,
        uint32_t binding,
        vk::DescriptorType descriptorType,
        std::span<const vk::DescriptorImageInfo> imageInfos,
        uint32_t arrayElement) -> DescriptorWriter&
    {
        validateDescriptorSet(destinationSet);

        if (imageInfos.empty())
        {
            throw std::runtime_error("Descriptor image write requires at least one image info");
        }

        this->imageWrites.push_back(ImageDescriptorWrite{
            .destinationSet = destinationSet,
            .binding = binding,
            .arrayElement = arrayElement,
            .descriptorType = descriptorType,
            .imageInfos = std::vector<vk::DescriptorImageInfo>{imageInfos.begin(), imageInfos.end()},
        });
        return *this;
    }

    auto DescriptorWriter::writeImage(
        vk::DescriptorSet destinationSet,
        uint32_t binding,
        vk::DescriptorType descriptorType,
        Image const& image,
        vk::ImageLayout layout,
        vk::Sampler sampler,
        uint32_t arrayElement) -> DescriptorWriter&
    {
        return writeImage(
            destinationSet,
            binding,
            descriptorType,
            image.descriptorInfo(layout, sampler),
            arrayElement);
    }

    auto DescriptorWriter::copy(
        vk::DescriptorSet sourceSet,
        uint32_t sourceBinding,
        vk::DescriptorSet destinationSet,
        uint32_t destinationBinding,
        uint32_t descriptorCount,
        uint32_t sourceArrayElement,
        uint32_t destinationArrayElement) -> DescriptorWriter&
    {
        if (sourceSet == VK_NULL_HANDLE || destinationSet == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Descriptor copy requires valid source and destination sets");
        }

        if (descriptorCount == 0)
        {
            throw std::runtime_error("Descriptor copy requires a non-zero descriptor count");
        }

        copies.push_back(vk::CopyDescriptorSet{
            .srcSet = sourceSet,
            .srcBinding = sourceBinding,
            .srcArrayElement = sourceArrayElement,
            .dstSet = destinationSet,
            .dstBinding = destinationBinding,
            .dstArrayElement = destinationArrayElement,
            .descriptorCount = descriptorCount,
        });
        return *this;
    }

    auto DescriptorWriter::update(vk::raii::Device const& device) const -> void
    {
        if (empty())
        {
            return;
        }

        std::vector<vk::WriteDescriptorSet> writes{};
        writes.reserve(bufferWrites.size() + imageWrites.size());

        for (BufferDescriptorWrite const& bufferWrite : bufferWrites)
        {
            writes.push_back(vk::WriteDescriptorSet{
                .dstSet = bufferWrite.destinationSet,
                .dstBinding = bufferWrite.binding,
                .dstArrayElement = bufferWrite.arrayElement,
                .descriptorCount = static_cast<uint32_t>(bufferWrite.bufferInfos.size()),
                .descriptorType = bufferWrite.descriptorType,
                .pBufferInfo = bufferWrite.bufferInfos.data(),
            });
        }

        for (ImageDescriptorWrite const& imageWrite : imageWrites)
        {
            writes.push_back(vk::WriteDescriptorSet{
                .dstSet = imageWrite.destinationSet,
                .dstBinding = imageWrite.binding,
                .dstArrayElement = imageWrite.arrayElement,
                .descriptorCount = static_cast<uint32_t>(imageWrite.imageInfos.size()),
                .descriptorType = imageWrite.descriptorType,
                .pImageInfo = imageWrite.imageInfos.data(),
            });
        }

        device.updateDescriptorSets(writes, copies);
    }

}  // namespace Aerkanis::Vk
