#include "vk/descriptor-set-layout.hpp"

#include <stdexcept>

namespace Aerkanis::Vk
{

    auto DescriptorSetLayout::addBinding(
        uint32_t binding,
        vk::DescriptorType descriptorType,
        vk::ShaderStageFlags stageFlags,
        uint32_t descriptorCount) -> DescriptorSetLayout&
    {
        if (descriptorCount == 0)
        {
            throw std::runtime_error("Descriptor set layout binding requires a non-zero descriptor count");
        }

        bindings.push_back(vk::DescriptorSetLayoutBinding{
            .binding = binding,
            .descriptorType = descriptorType,
            .descriptorCount = descriptorCount,
            .stageFlags = stageFlags,
        });
        return *this;
    }

    auto DescriptorSetLayout::init(
        vk::raii::Device const& device,
        std::span<const vk::DescriptorSetLayoutBinding> nextBindings,
        vk::DescriptorSetLayoutCreateFlags nextFlags) -> void
    {
        bindings.assign(nextBindings.begin(), nextBindings.end());
        build(device, nextFlags);
    }

    auto DescriptorSetLayout::build(
        vk::raii::Device const& device,
        vk::DescriptorSetLayoutCreateFlags nextFlags) -> void
    {
        layout.clear();
        flags = nextFlags;

        if (bindings.empty())
        {
            throw std::runtime_error("DescriptorSetLayout requires at least one binding");
        }

        vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{
            .flags = flags,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };

        layout = vk::raii::DescriptorSetLayout(device, layoutCreateInfo);
    }

    auto DescriptorSetLayout::clearBindings() -> DescriptorSetLayout&
    {
        bindings.clear();
        return *this;
    }

    auto DescriptorSetLayout::shutdown() noexcept -> void
    {
        layout.clear();
        bindings.clear();
        flags = {};
    }

    auto DescriptorSetLayout::handle() const noexcept -> vk::DescriptorSetLayout
    {
        return static_cast<vk::DescriptorSetLayout>(*layout);
    }

}  // namespace Aerkanis::Vk
