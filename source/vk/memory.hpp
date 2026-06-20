#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

#include "core/context.hpp"

namespace Aerkanis::Vk
{

    auto findMemoryType(
        Context const& context,
        uint32_t typeFilter,
        vk::MemoryPropertyFlags requiredProperties) -> uint32_t;

}  // namespace Aerkanis::Vk
