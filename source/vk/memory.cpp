#include "vk/memory.hpp"

#include <stdexcept>

namespace Aerkanis::Vk
{

    auto findMemoryType(
        Context const& context,
        uint32_t typeFilter,
        vk::MemoryPropertyFlags requiredProperties) -> uint32_t
    {
        const vk::PhysicalDeviceMemoryProperties memoryProperties =
            context.physicalDevice.getMemoryProperties();

        for (uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index)
        {
            const bool typeMatches = (typeFilter & (1U << index)) != 0;
            const bool propertiesMatch =
                (memoryProperties.memoryTypes[index].propertyFlags & requiredProperties) == requiredProperties;

            if (typeMatches && propertiesMatch)
            {
                return index;
            }
        }

        throw std::runtime_error("failed to find suitable Vulkan memory type");
    }

}  // namespace Aerkanis::Vk
