#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "scene/scene.hpp"

struct ImGuiContext;

namespace Aerkanis
{

    struct Context;
    struct Swapchain;
    struct Window;

    struct GuiPass
    {
        ImGuiContext* imguiContext{};
        VkFormat colorAttachmentFormat{VK_FORMAT_UNDEFINED};
        VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
        bool initialized{false};
        bool glfwBackendInitialized{false};
        bool vulkanBackendInitialized{false};

        auto init(Window const& window, Context& context, Swapchain const& swapchain) -> bool;
        auto shutdown() noexcept -> void;
        auto beginFrame() -> void;
        auto wantsMouseCapture() const noexcept -> bool;
        auto wantsKeyboardCapture() const noexcept -> bool;
        auto drawSceneControls(Scene::SceneState& sceneState, bool showTriangleControls) -> void;
        auto render(vk::raii::CommandBuffer const& commandBuffer) -> void;
    };

}  // namespace Aerkanis
