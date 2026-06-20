#include "vk/descriptor-pool.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Aerkanis::Vk
{

    namespace
    {

        auto normalizePoolFlags(vk::DescriptorPoolCreateFlags flags) -> vk::DescriptorPoolCreateFlags
        {
            return flags | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        }

        auto validatePoolSizes(std::span<const vk::DescriptorPoolSize> sizes) -> void
        {
            if (sizes.empty())
            {
                throw std::runtime_error("DescriptorPool requires at least one pool size");
            }

            for (vk::DescriptorPoolSize const& size : sizes)
            {
                if (size.descriptorCount == 0)
                {
                    throw std::runtime_error("DescriptorPool size entries require non-zero descriptor counts");
                }
            }
        }

    }  // namespace

    auto DescriptorPool::init(
        vk::raii::Device const& device,
        uint32_t nextMaxSets,
        std::span<const vk::DescriptorPoolSize> nextPoolSizes,
        vk::DescriptorPoolCreateFlags nextFlags) -> void
    {
        shutdown();

        if (nextMaxSets == 0)
        {
            throw std::runtime_error("DescriptorPool requires a non-zero max set count");
        }

        validatePoolSizes(nextPoolSizes);

        maxSets = nextMaxSets;
        poolSizes.assign(nextPoolSizes.begin(), nextPoolSizes.end());
        flags = normalizePoolFlags(nextFlags);

        vk::DescriptorPoolCreateInfo poolCreateInfo{
            .flags = flags,
            .maxSets = maxSets,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data(),
        };

        pool = vk::raii::DescriptorPool(device, poolCreateInfo);
    }

    auto DescriptorPool::initWithRatios(
        vk::raii::Device const& device,
        uint32_t nextMaxSets,
        std::span<const DescriptorPoolRatio> ratios,
        vk::DescriptorPoolCreateFlags nextFlags) -> void
    {
        if (ratios.empty())
        {
            throw std::runtime_error("DescriptorPool ratios cannot be empty");
        }

        std::vector<vk::DescriptorPoolSize> sizes{};
        sizes.reserve(ratios.size());

        for (DescriptorPoolRatio const& ratio : ratios)
        {
            if (ratio.ratio <= 0.0F)
            {
                throw std::runtime_error("DescriptorPool ratios must be positive");
            }

            const auto descriptorCount =
                static_cast<uint32_t>(std::max(1.0F, std::ceil(ratio.ratio * static_cast<float>(nextMaxSets))));
            sizes.push_back(vk::DescriptorPoolSize{
                .type = ratio.descriptorType,
                .descriptorCount = descriptorCount,
            });
        }

        init(device, nextMaxSets, sizes, nextFlags);
    }

    auto DescriptorPool::allocate(vk::raii::Device const& device, vk::DescriptorSetLayout layout) const
        -> vk::raii::DescriptorSet
    {
        const std::array layouts{layout};
        std::vector<vk::raii::DescriptorSet> descriptorSets = allocateMany(device, layouts);
        return std::move(descriptorSets.front());
    }

    auto DescriptorPool::allocate(vk::raii::Device const& device, DescriptorSetLayout const& layout) const
        -> vk::raii::DescriptorSet
    {
        return allocate(device, layout.handle());
    }

    auto DescriptorPool::allocateMany(
        vk::raii::Device const& device,
        std::span<const vk::DescriptorSetLayout> layouts) const -> std::vector<vk::raii::DescriptorSet>
    {
        if (static_cast<vk::DescriptorPool>(*pool) == VK_NULL_HANDLE)
        {
            throw std::runtime_error("DescriptorPool::allocateMany requires an initialized pool");
        }

        if (layouts.empty())
        {
            return {};
        }

        vk::DescriptorSetAllocateInfo allocateInfo{
            .descriptorPool = handle(),
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
        };

        return device.allocateDescriptorSets(allocateInfo);
    }

    auto DescriptorPool::reset() -> void
    {
        if (static_cast<vk::DescriptorPool>(*pool) == VK_NULL_HANDLE)
        {
            return;
        }

        pool.reset();
    }

    auto DescriptorPool::shutdown() noexcept -> void
    {
        pool.clear();
        poolSizes.clear();
        maxSets = 0;
        flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    }

    auto DescriptorPool::handle() const noexcept -> vk::DescriptorPool
    {
        return static_cast<vk::DescriptorPool>(*pool);
    }

}  // namespace Aerkanis::Vk
