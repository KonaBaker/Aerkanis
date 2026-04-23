#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Aerkanis
{

    namespace Details
    {
        auto validationEnabledByDefault() -> bool;
        auto hasLayer(std::vector<vk::LayerProperties> const& layers, const char* name) -> bool;
        auto hasExtension(std::vector<vk::ExtensionProperties> const& extensions, const char* name) -> bool;
    }  // namespace Details

    struct ContextConfig
    {
        std::string appName = "Aerkanis";
        std::string engineName = "Aerkanis";
        bool enableValidation = Details::validationEnabledByDefault();
    };

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily{};
        std::optional<uint32_t> computeFamily{};
        std::optional<uint32_t> transferFamily{};
        std::optional<uint32_t> presentFamily{};
    };

    struct Context
    {
    #if VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL
        vk::raii::Context loaderContext{};
    #else
        vk::raii::Context loaderContext{&vkGetInstanceProcAddr};
    #endif
        vk::raii::Instance instance{nullptr};
        vk::raii::DebugUtilsMessengerEXT debugMessenger{nullptr};
        vk::raii::PhysicalDevice physicalDevice{nullptr};
        vk::raii::Device device{nullptr};
        vk::raii::Queue graphicsQueue{nullptr};
        vk::raii::Queue computeQueue{nullptr};
        vk::raii::Queue transferQueue{nullptr};
        vk::raii::Queue presentQueue{nullptr};
        QueueFamilyIndices queueFamilies{};
        bool validationEnabled{false};

        auto init(ContextConfig config = {}) -> bool;
        auto initDevice(const vk::raii::SurfaceKHR& surface) -> bool;
        auto shutdown() noexcept -> void;
    };

}  // namespace Aerkanis
