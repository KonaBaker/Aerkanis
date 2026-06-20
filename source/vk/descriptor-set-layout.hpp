#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Aerkanis::Vk
{

    struct DescriptorSetLayout
    {
        vk::raii::DescriptorSetLayout layout{nullptr};
        std::vector<vk::DescriptorSetLayoutBinding> bindings{};
        vk::DescriptorSetLayoutCreateFlags flags{};

        auto addBinding(
            uint32_t binding,
            vk::DescriptorType descriptorType,
            vk::ShaderStageFlags stageFlags,
            uint32_t descriptorCount = 1) -> DescriptorSetLayout&;
        auto init(
            vk::raii::Device const& device,
            std::span<const vk::DescriptorSetLayoutBinding> nextBindings,
            vk::DescriptorSetLayoutCreateFlags nextFlags = {}) -> void;
        auto build(vk::raii::Device const& device, vk::DescriptorSetLayoutCreateFlags nextFlags = {}) -> void;
        auto clearBindings() -> DescriptorSetLayout&;
        auto shutdown() noexcept -> void;
        auto handle() const noexcept -> vk::DescriptorSetLayout;
    };

}  // namespace Aerkanis::Vk
