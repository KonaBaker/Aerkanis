#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Aerkanis
{

    struct Window;

    struct ContextConfig
    {
        std::string appName = "Aerkanis";
        std::string engineName = "Aerkanis";
#ifndef NDEBUG
        bool enableValidation = true;
#else
        bool enableValidation = false;
#endif
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
        vk::raii::SurfaceKHR surface{nullptr};
        vk::raii::DebugUtilsMessengerEXT debugMessenger{nullptr};
        vk::raii::PhysicalDevice physicalDevice{nullptr};
        vk::raii::Device device{nullptr};
        vk::raii::Queue graphicsQueue{nullptr};
        vk::raii::Queue computeQueue{nullptr};
        vk::raii::Queue transferQueue{nullptr};
        vk::raii::Queue presentQueue{nullptr};
        QueueFamilyIndices queueFamilies{};
        bool validationEnabled{false};

        auto init(Window const& window, ContextConfig config = {}) -> void;
        auto shutdown() noexcept -> void;

    private:
        static auto debugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
            vk::DebugUtilsMessageTypeFlagsEXT messageType,
            const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
            void* userData) -> vk::Bool32;
        auto createInstance(ContextConfig config) -> void;
        auto createSurface(Window const& window) -> void;
        auto pickPhysicalDevice() -> void;
        auto createLogicalDevice() -> void;
    };

}  // namespace Aerkanis
