#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "vk/descriptor-set-layout.hpp"

namespace Aerkanis::Vk
{

    struct DescriptorPoolRatio
    {
        vk::DescriptorType descriptorType{};
        float ratio{};
    };

    struct DescriptorPool
    {
        vk::raii::DescriptorPool pool{nullptr};
        std::vector<vk::DescriptorPoolSize> poolSizes{};
        uint32_t maxSets{};
        vk::DescriptorPoolCreateFlags flags{vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet};

        auto init(
            vk::raii::Device const& device,
            uint32_t nextMaxSets,
            std::span<const vk::DescriptorPoolSize> nextPoolSizes,
            vk::DescriptorPoolCreateFlags nextFlags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            -> void;
        auto initWithRatios(
            vk::raii::Device const& device,
            uint32_t nextMaxSets,
            std::span<const DescriptorPoolRatio> ratios,
            vk::DescriptorPoolCreateFlags nextFlags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            -> void;
        auto allocate(vk::raii::Device const& device, vk::DescriptorSetLayout layout) const
            -> vk::raii::DescriptorSet;
        auto allocate(vk::raii::Device const& device, DescriptorSetLayout const& layout) const
            -> vk::raii::DescriptorSet;
        auto allocateMany(vk::raii::Device const& device, std::span<const vk::DescriptorSetLayout> layouts) const
            -> std::vector<vk::raii::DescriptorSet>;
        auto reset() -> void;
        auto shutdown() noexcept -> void;
        auto handle() const noexcept -> vk::DescriptorPool;
    };

}  // namespace Aerkanis::Vk
