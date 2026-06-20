#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include "app/window.hpp"
#include "core/context.hpp"
#include "core/frame-data.hpp"
#include "core/swapchain.hpp"
#include "vk/pipeline-builder.hpp"
#include "vk/shader.hpp"

namespace Aerkanis
{

    struct TriangleRenderer
    {
        Context context{};
        Swapchain swapchain{};
        FrameSet frames{};
        Vk::ShaderModule triangleShader{};
        Vk::PipelineBuildResult trianglePipeline{};
        std::vector<bool> swapchainImageInitialized{};
        bool initialized{false};

        auto init(Window const& window) -> bool;
        auto shutdown() noexcept -> void;
        auto drawFrame(Window const& window, bool& framebufferResized) -> bool;

    private:
        auto createTrianglePipeline() -> void;
        auto recreateSwapchain(Window const& window) -> bool;
        auto recordTriangleCommands(FrameData& frame, uint32_t imageIndex) -> bool;
    };

}  // namespace Aerkanis
