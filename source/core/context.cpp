#include "core/context.hpp"

#include <array>
#include <iostream>
#include <limits>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>

#include "app/window.hpp"

namespace Aerkanis
{

    auto Context::debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT,
        const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
        void*) -> vk::Bool32
    {
        const char* severityLabel = "UNKNOWN";
        switch (severity)
        {
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                severityLabel = "VERBOSE";
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                severityLabel = "INFO";
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                severityLabel = "WARNING";
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                severityLabel = "ERROR";
                break;
            default:
                break;
        }

        std::cerr << "[Vulkan][" << severityLabel << "] "
                  << (callbackData != nullptr && callbackData->pMessage != nullptr
                          ? callbackData->pMessage
                          : "unknown")
                  << '\n';
        return VK_FALSE;
    }

    auto Context::init(Window const& window, ContextConfig config) -> void
    {
        shutdown();

        try
        {
            createInstance(config);

            if (validationEnabled)
            {
                const vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{
                    .messageSeverity =
                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                    .messageType =
                        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                    .pfnUserCallback = &Context::debugCallback,
                };
                debugMessenger = vk::raii::DebugUtilsMessengerEXT(instance, debugCreateInfo);
            }

            createSurface(window);
            pickPhysicalDevice();
            createLogicalDevice();
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Vulkan] context init failed: " << exception.what() << '\n';
            shutdown();
        }
    }

    auto Context::createInstance(ContextConfig config) -> void
    {
#if VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
#else
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
#endif

        validationEnabled = config.enableValidation;

        const std::vector<vk::LayerProperties> availableLayers = loaderContext.enumerateInstanceLayerProperties();
        if (validationEnabled)
        {
            bool validationLayerAvailable = false;
            for (const vk::LayerProperties& layer : availableLayers)
            {
                if (std::string_view(layer.layerName) == "VK_LAYER_KHRONOS_validation")
                {
                    validationLayerAvailable = true;
                    break;
                }
            }

            if (!validationLayerAvailable)
            {
                std::cerr << "[Vulkan] validation layer not available, continuing without it\n";
                validationEnabled = false;
            }
        }

        const std::vector<vk::ExtensionProperties> availableExtensions =
            loaderContext.enumerateInstanceExtensionProperties();
        if (validationEnabled)
        {
            bool debugUtilsAvailable = false;
            for (const vk::ExtensionProperties& extension : availableExtensions)
            {
                if (std::string_view(extension.extensionName) == VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                {
                    debugUtilsAvailable = true;
                    break;
                }
            }

            if (!debugUtilsAvailable)
            {
                std::cerr << "[Vulkan] debug utils extension not available, disabling validation messenger\n";
                validationEnabled = false;
            }
        }

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (glfwExtensions == nullptr || glfwExtensionCount == 0)
        {
            throw std::runtime_error("glfwGetRequiredInstanceExtensions returned no extensions");
        }

        std::vector<const char*> instanceExtensions{};
        instanceExtensions.reserve(static_cast<std::size_t>(glfwExtensionCount) + (validationEnabled ? 1U : 0U));
        for (uint32_t index = 0; index < glfwExtensionCount; ++index)
        {
            instanceExtensions.push_back(glfwExtensions[index]);
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

        vk::InstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

        std::vector<const char*> validationLayers{};
        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (validationEnabled)
        {
            debugCreateInfo.messageSeverity =
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            debugCreateInfo.messageType =
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            debugCreateInfo.pfnUserCallback = &Context::debugCallback;
            validationLayers.push_back("VK_LAYER_KHRONOS_validation");
            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
            instanceCreateInfo.pNext = &debugCreateInfo;
        }

        instance = vk::raii::Instance(loaderContext, instanceCreateInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    }

    auto Context::createSurface(Window const& window) -> void
    {
        VkSurfaceKHR rawSurface{};
        if (glfwCreateWindowSurface(*instance, window.nativeWindow, nullptr, &rawSurface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface");
        }

        surface = vk::raii::SurfaceKHR(instance, rawSurface);
    }

    auto Context::pickPhysicalDevice() -> void
    {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        if (physicalDevices.empty())
        {
            throw std::runtime_error("no physical devices found");
        }

        const vk::SurfaceKHR rawSurface = static_cast<vk::SurfaceKHR>(*surface);

        auto hasExtension = [](const std::vector<vk::ExtensionProperties>& extensions, const char* name) -> bool
        {
            for (const vk::ExtensionProperties& extension : extensions)
            {
                if (std::string_view(extension.extensionName) == name)
                {
                    return true;
                }
            }
            return false;
        };

        auto findQueueFamilyIndex = [](const std::vector<vk::QueueFamilyProperties>& families, auto&& predicate)
            -> std::optional<uint32_t>
        {
            for (uint32_t familyIndex = 0; familyIndex < families.size(); ++familyIndex)
            {
                if (predicate(familyIndex, families[familyIndex]))
                {
                    return familyIndex;
                }
            }
            return std::nullopt;
        };

        int bestScore = std::numeric_limits<int>::min();
        std::optional<std::size_t> bestIndex{};
        QueueFamilyIndices bestFamilies{};

        for (std::size_t index = 0; index < physicalDevices.size(); ++index)
        {
            const vk::raii::PhysicalDevice& candidate = physicalDevices[index];
            const std::vector<vk::ExtensionProperties> deviceExtensions =
                candidate.enumerateDeviceExtensionProperties();
            if (!hasExtension(deviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            {
                continue;
            }

            const std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
                candidate.getQueueFamilyProperties();

            QueueFamilyIndices families{};
            families.graphicsFamily = findQueueFamilyIndex(queueFamilyProperties, [](uint32_t, const vk::QueueFamilyProperties& family) {
                return family.queueCount > 0 && static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics);
            });
            families.presentFamily = findQueueFamilyIndex(queueFamilyProperties, [&](uint32_t familyIndex, const vk::QueueFamilyProperties&) {
                return candidate.getSurfaceSupportKHR(familyIndex, rawSurface) == VK_TRUE;
            });
            families.computeFamily = findQueueFamilyIndex(queueFamilyProperties, [](uint32_t, const vk::QueueFamilyProperties& family) {
                return family.queueCount > 0 &&
                       static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eCompute) &&
                       !static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics);
            });
            families.transferFamily = findQueueFamilyIndex(queueFamilyProperties, [](uint32_t, const vk::QueueFamilyProperties& family) {
                return family.queueCount > 0 &&
                       static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eTransfer) &&
                       !static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eGraphics) &&
                       !static_cast<bool>(family.queueFlags & vk::QueueFlagBits::eCompute);
            });

            if (!families.computeFamily.has_value() && families.graphicsFamily.has_value())
            {
                families.computeFamily = families.graphicsFamily;
            }
            if (!families.transferFamily.has_value() && families.computeFamily.has_value())
            {
                families.transferFamily = families.computeFamily;
            }
            if (!families.transferFamily.has_value() && families.graphicsFamily.has_value())
            {
                families.transferFamily = families.graphicsFamily;
            }

            if (!families.graphicsFamily.has_value() || !families.presentFamily.has_value())
            {
                continue;
            }

            if (candidate.getSurfaceFormatsKHR(rawSurface).empty() ||
                candidate.getSurfacePresentModesKHR(rawSurface).empty())
            {
                continue;
            }

            const vk::PhysicalDeviceProperties properties = candidate.getProperties();
            int score = 0;
            switch (properties.deviceType)
            {
                case vk::PhysicalDeviceType::eDiscreteGpu:
                    score = 4;
                    break;
                case vk::PhysicalDeviceType::eIntegratedGpu:
                    score = 3;
                    break;
                case vk::PhysicalDeviceType::eVirtualGpu:
                    score = 2;
                    break;
                case vk::PhysicalDeviceType::eCpu:
                    score = 1;
                    break;
                default:
                    break;
            }

            if (!bestIndex.has_value() || score > bestScore)
            {
                bestScore = score;
                bestIndex = index;
                bestFamilies = families;
            }
        }

        if (!bestIndex.has_value())
        {
            throw std::runtime_error("no suitable physical device found");
        }

        physicalDevice = std::move(physicalDevices[bestIndex.value()]);
        queueFamilies = bestFamilies;
    }

    auto Context::createLogicalDevice() -> void
    {
        if (static_cast<vk::PhysicalDevice>(*physicalDevice) == VK_NULL_HANDLE)
        {
            throw std::runtime_error("createLogicalDevice requires a valid physical device");
        }

        if (!queueFamilies.graphicsFamily.has_value() || !queueFamilies.presentFamily.has_value())
        {
            throw std::runtime_error("createLogicalDevice requires selected queue families");
        }

        const std::vector<vk::ExtensionProperties> deviceExtensionsAvailable =
            physicalDevice.enumerateDeviceExtensionProperties();
        bool swapchainExtensionAvailable = false;
        for (const vk::ExtensionProperties& extension : deviceExtensionsAvailable)
        {
            if (std::string_view(extension.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME)
            {
                swapchainExtensionAvailable = true;
                break;
            }
        }
        if (!swapchainExtensionAvailable)
        {
            throw std::runtime_error("swapchain extension not available");
        }

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

            queueCreateInfos.push_back(vk::DeviceQueueCreateInfo{
                .queueFamilyIndex = familyIndex.value(),
                .queueCount = 1,
                .pQueuePriorities = queuePriorities.data(),
            });
        };

        appendQueueInfo(queueFamilies.graphicsFamily);
        appendQueueInfo(queueFamilies.computeFamily);
        appendQueueInfo(queueFamilies.transferFamily);
        appendQueueInfo(queueFamilies.presentFamily);

        const vk::PhysicalDeviceFeatures supportedFeatures = physicalDevice.getFeatures();
        vk::PhysicalDeviceFeatures enabledFeatures{};
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
    }

    auto Context::shutdown() noexcept -> void
    {
        try
        {
            if (static_cast<vk::Device>(*device) != VK_NULL_HANDLE)
            {
                device.waitIdle();
            }
        }
        catch (...)
        {
        }

        presentQueue.clear();
        transferQueue.clear();
        computeQueue.clear();
        graphicsQueue.clear();
        device.clear();
        debugMessenger.clear();
        physicalDevice.clear();
        surface.clear();
        instance.clear();
        queueFamilies = {};
        validationEnabled = false;
    }

}  // namespace Aerkanis
