#pragma once

#include <chrono>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "app/window.hpp"
#include "core/context.hpp"
#include "core/frame-data.hpp"
#include "core/swapchain.hpp"
#include "render/gui-pass.hpp"
#include "scene/camera-controller.hpp"
#include "scene/scene.hpp"
#include "vk/pipeline-builder.hpp"
#include "vk/shader.hpp"

namespace Aerkanis
{

    struct TriangleRenderer
    {
        Context context{};
        Swapchain swapchain{};
        FrameSet frames{};
        Scene::SceneState sceneState{};
        Scene::CameraController cameraController{};
        GuiPass guiPass{};
        Vk::ShaderModule triangleShader{};
        Vk::PipelineBuildResult trianglePipeline{};
        std::vector<bool> swapchainImageInitialized{};
        std::chrono::steady_clock::time_point previousFrameTime{};
        bool initialized{false};

        auto init(Window const& window) -> bool;
        auto shutdown() noexcept -> void;
        auto drawFrame(Window const& window, bool& framebufferResized) -> bool;

    private:
        auto createTrianglePipeline() -> void;
        auto recreateSwapchain(Window const& window) -> bool;
        auto recordTriangleCommands(
            FrameData& frame,
            uint32_t imageIndex,
            Window const& window,
            float deltaSeconds) -> bool;
    };

}  // namespace Aerkanis
