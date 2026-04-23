#include "core/context.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>

namespace Aerkanis::Details
{

    auto toString(vk::DebugUtilsMessageSeverityFlagBitsEXT severity) -> const char*
    {
        switch (severity)
        {
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                return "VERBOSE";
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                return "INFO";
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                return "WARNING";
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }

    auto makeQueueInfo(uint32_t familyIndex, const float* priorities) -> vk::DeviceQueueCreateInfo
    {
        return vk::DeviceQueueCreateInfo{
            .queueFamilyIndex = familyIndex,
            .queueCount = 1,
            .pQueuePriorities = priorities,
        };
    }

    auto debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT,
        const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
        void*) -> vk::Bool32
        {
        std::cerr << "[Vulkan][" << toString(severity)
                  << "] " << (callbackData != nullptr && callbackData->pMessage != nullptr
                                   ? callbackData->pMessage
                                   : "unknown")
                  << '\n';
        return VK_FALSE;
    }

    struct QueuePickResult
    {
        QueueFamilyIndices families{};
        bool valid{false};
    };

    auto scoreDevice(vk::PhysicalDeviceType type) -> int
    {
        switch (type)
        {
            case vk::PhysicalDeviceType::eDiscreteGpu:
                return 4;
            case vk::PhysicalDeviceType::eIntegratedGpu:
                return 3;
            case vk::PhysicalDeviceType::eVirtualGpu:
                return 2;
            case vk::PhysicalDeviceType::eCpu:
                return 1;
            default:
                return 0;
        }
    }

    auto pickQueueFamilies(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) -> QueuePickResult
    {
        QueuePickResult result{};
        const std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

        for (uint32_t familyIndex = 0; familyIndex < queueFamilies.size(); ++familyIndex)
        {
            const vk::QueueFamilyProperties& family = queueFamilies[familyIndex];
            if (family.queueCount == 0)
            {
                continue;
            }

            const bool supportsGraphics = static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics);
            const bool supportsPresent = device.getSurfaceSupportKHR(familyIndex, surface) == VK_TRUE;

            if (supportsGraphics && !result.families.graphicsFamily.has_value())
            {
                result.families.graphicsFamily = familyIndex;
            }
            if (supportsPresent && !result.families.presentFamily.has_value())
            {
                result.families.presentFamily = familyIndex;
            }
        }

        for (uint32_t familyIndex = 0; familyIndex < queueFamilies.size(); ++familyIndex)
        {
            const vk::QueueFamilyProperties& family = queueFamilies[familyIndex];
            if (family.queueCount == 0 || !static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eCompute))
            {
                continue;
            }

            if (static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics))
            {
                continue;
            }

            result.families.computeFamily = familyIndex;
            break;
        }

        for (uint32_t familyIndex = 0; familyIndex < queueFamilies.size(); ++familyIndex)
        {
            const vk::QueueFamilyProperties& family = queueFamilies[familyIndex];
            if (family.queueCount == 0 || !static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eTransfer))
            {
                continue;
            }

            if (static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics) ||
                static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eCompute))
                {
                continue;
            }

            result.families.transferFamily = familyIndex;
            break;
        }

        if (!result.families.computeFamily.has_value() && result.families.graphicsFamily.has_value())
        {
            result.families.computeFamily = result.families.graphicsFamily;
        }
        if (!result.families.transferFamily.has_value() && result.families.computeFamily.has_value())
        {
            result.families.transferFamily = result.families.computeFamily;
        }
        if (!result.families.transferFamily.has_value() && result.families.graphicsFamily.has_value())
        {
            result.families.transferFamily = result.families.graphicsFamily;
        }

        result.valid = result.families.graphicsFamily.has_value() && result.families.presentFamily.has_value();
        return result;
    }

    auto deviceHasExtensions(const vk::raii::PhysicalDevice& device, std::vector<const char*> const& requiredExtensions) -> bool
    {
        const std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();
        for (const char* requiredExtension : requiredExtensions)
        {
            if (!hasExtension(availableExtensions, requiredExtension))
            {
                return false;
            }
        }
        return true;
    }

    auto isDeviceSuitable(const vk::raii::PhysicalDevice& device, const vk::raii::SurfaceKHR& surface) -> std::optional<QueuePickResult>
    {
        const std::vector<const char*> deviceExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        if (!deviceHasExtensions(device, deviceExtensions))
        {
            return std::nullopt;
        }

        const auto queuePick = pickQueueFamilies(device, surface);
        if (!queuePick.valid)
        {
            return std::nullopt;
        }

        const std::vector<vk::SurfaceFormatKHR> surfaceFormats = device.getSurfaceFormatsKHR(surface);
        const std::vector<vk::PresentModeKHR> surfacePresentModes = device.getSurfacePresentModesKHR(surface);
        if (surfaceFormats.empty() || surfacePresentModes.empty())
        {
            return std::nullopt;
        }

        return queuePick;
    }

auto validationEnabledByDefault() -> bool
{
#ifndef NDEBUG
    return true;
#else
    return false;
#endif
}

auto hasLayer(std::vector<vk::LayerProperties> const& layers, const char* name) -> bool
{
    for (const vk::LayerProperties& layer : layers)
    {
        if (std::string_view(layer.layerName) == std::string_view(name))
        {
            return true;
        }
    }
    return false;
}

auto hasExtension(std::vector<vk::ExtensionProperties> const& extensions, const char* name) -> bool
{
    for (const vk::ExtensionProperties& extension : extensions)
    {
        if (std::string_view(extension.extensionName) == std::string_view(name))
        {
            return true;
        }
    }
    return false;
}

}  // namespace Aerkanis::Details

namespace Aerkanis
{

auto Context::init(ContextConfig config) -> bool
{
    shutdown();

    try
    {
        #if VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL
                VULKAN_HPP_DEFAULT_DISPATCHER.init();
        #else
                VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        #endif
                validationEnabled = config.enableValidation;
        #ifndef NDEBUG
                validationEnabled = true;
        #endif

        const std::vector<vk::LayerProperties> availableLayers = loaderContext.enumerateInstanceLayerProperties();
        if (validationEnabled && !Details::hasLayer(availableLayers, "VK_LAYER_KHRONOS_validation"))
        {
            std::cerr << "[Vulkan] validation layer not available, continuing without it\n";
            validationEnabled = false;
        }

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (glfwExtensions == nullptr || glfwExtensionCount == 0)
        {
            std::cerr << "[Vulkan] glfwGetRequiredInstanceExtensions returned no extensions\n";
            return false;
        }

        std::vector<const char*> instanceExtensions{};
        instanceExtensions.reserve(static_cast<std::size_t>(glfwExtensionCount) + 1);
        for (uint32_t index = 0; index < glfwExtensionCount; ++index)
        {
            instanceExtensions.push_back(glfwExtensions[index]);
        }

        if (validationEnabled && !Details::hasExtension(loaderContext.enumerateInstanceExtensionProperties(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            std::cerr << "[Vulkan] debug utils extension not available, disabling validation messenger\n";
            validationEnabled = false;
        }

        if (validationEnabled)
        {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        vk::ApplicationInfo applicationInfo{
            .pApplicationName = config.appName.c_str(),
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = config.engineName.c_str(),
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_4,
        };

        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        debugCreateInfo.messageSeverity =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugCreateInfo.messageType =
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugCreateInfo.pfnUserCallback = &Details::debugCallback;

        vk::InstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.enabledLayerCount = 0;
        instanceCreateInfo.ppEnabledLayerNames = nullptr;
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

        std::vector<const char*> validationLayers{};
        if (validationEnabled)
        {
            validationLayers.push_back("VK_LAYER_KHRONOS_validation");
            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
            instanceCreateInfo.pNext = &debugCreateInfo;
        }

        instance = vk::raii::Instance(loaderContext, instanceCreateInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

        if (validationEnabled)
        {
            debugMessenger = vk::raii::DebugUtilsMessengerEXT(instance, debugCreateInfo);
        }

        return true;
    } catch (const std::exception& exception) {
        std::cerr << "[Vulkan] context init failed: " << exception.what() << '\n';
        shutdown();
        return false;
    }
}

auto Context::initDevice(const vk::raii::SurfaceKHR& surface) -> bool
{
    if (static_cast<vk::Instance>(*instance) == VK_NULL_HANDLE)
    {
        std::cerr << "[Vulkan] initDevice requires a valid instance (call Context::init first)\n";
        return false;
    }

    try
    {
        int bestScore = std::numeric_limits<int>::min();
        std::optional<std::size_t> bestIndex{};
        std::optional<Details::QueuePickResult> bestQueuePick{};

        const std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        if (physicalDevices.empty())
        {
            std::cerr << "[Vulkan] no physical devices found\n";
            shutdown();
            return false;
        }

        for (std::size_t index = 0; index < physicalDevices.size(); ++index)
        {
            const vk::raii::PhysicalDevice& candidate = physicalDevices[index];
            const auto suitability = Details::isDeviceSuitable(candidate, surface);
            if (!suitability.has_value())
            {
                continue;
            }

            const vk::PhysicalDeviceProperties properties = candidate.getProperties();
            const int score = Details::scoreDevice(properties.deviceType);
            if (score <= bestScore)
            {
                continue;
            }

            bestScore = score;
            bestIndex = index;
            bestQueuePick = suitability;
        }

        if (!bestQueuePick.has_value())
        {
            std::cerr << "[Vulkan] no suitable physical device found\n";
            shutdown();
            return false;
        }

        physicalDevice = std::move(physicalDevices[bestIndex.value()]);
        queueFamilies = bestQueuePick->families;

        static constexpr std::array<float, 1> queuePriorities{1.0f};
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{};
        queueCreateInfos.reserve(4);

        const auto appendQueueInfo = [&](std::optional<uint32_t> familyIndex) {
            if (!familyIndex.has_value())
            {
                return;
            }

            for (const vk::DeviceQueueCreateInfo& info : queueCreateInfos)
            {
                if (info.queueFamilyIndex == familyIndex.value())
                {
                    return;
                }
            }

            queueCreateInfos.push_back(Details::makeQueueInfo(familyIndex.value(), queuePriorities.data()));
        };

        appendQueueInfo(queueFamilies.graphicsFamily);
        appendQueueInfo(queueFamilies.computeFamily);
        appendQueueInfo(queueFamilies.transferFamily);
        appendQueueInfo(queueFamilies.presentFamily);

        const std::vector<vk::ExtensionProperties> deviceExtensionsAvailable = physicalDevice.enumerateDeviceExtensionProperties();
        if (!Details::hasExtension(deviceExtensionsAvailable, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        {
            std::cerr << "[Vulkan] swapchain extension not available\n";
            shutdown();
            return false;
        }

        vk::PhysicalDeviceFeatures enabledFeatures{};
        const vk::PhysicalDeviceFeatures supportedFeatures = physicalDevice.getFeatures();
        if (supportedFeatures.samplerAnisotropy == VK_TRUE)
        {
            enabledFeatures.samplerAnisotropy = VK_TRUE;
        }

        static constexpr std::array<const char*, 1> deviceExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        vk::DeviceCreateInfo deviceCreateInfo{
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos = queueCreateInfos.data(),
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &enabledFeatures,
        };

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

        if (queueFamilies.graphicsFamily.has_value())
        {
            graphicsQueue = device.getQueue(queueFamilies.graphicsFamily.value(), 0);
        }
        if (queueFamilies.computeFamily.has_value())
        {
            computeQueue = device.getQueue(queueFamilies.computeFamily.value(), 0);
        }
        if (queueFamilies.transferFamily.has_value())
        {
            transferQueue = device.getQueue(queueFamilies.transferFamily.value(), 0);
        }
        if (queueFamilies.presentFamily.has_value())
        {
            presentQueue = device.getQueue(queueFamilies.presentFamily.value(), 0);
        }

        return true;
    } catch (const std::exception& exception) {
        std::cerr << "[Vulkan] initDevice failed: " << exception.what() << '\n';
        return false;
    }
}

auto Context::shutdown() noexcept -> void
{
    try
    {
        if (static_cast<vk::Device>(*device) != VK_NULL_HANDLE)
        {
            device.waitIdle();
        }
    } catch (...) { }

    presentQueue.clear();
    transferQueue.clear();
    computeQueue.clear();
    graphicsQueue.clear();
    device.clear();
    debugMessenger.clear();
    physicalDevice.clear();
    instance.clear();
    queueFamilies = {};
    validationEnabled = false;
}

}  // namespace Aerkanis
