#include "render/gui-pass.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "app/window.hpp"
#include "core/context.hpp"
#include "core/swapchain.hpp"

#if defined(AERKANIS_IMGUI)
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#endif

namespace Aerkanis
{

#if defined(AERKANIS_IMGUI)
    namespace
    {

        auto rawInstance(Context const& context) -> VkInstance
        {
            return static_cast<VkInstance>(static_cast<vk::Instance>(*context.instance));
        }

        auto rawPhysicalDevice(Context const& context) -> VkPhysicalDevice
        {
            return static_cast<VkPhysicalDevice>(static_cast<vk::PhysicalDevice>(*context.physicalDevice));
        }

        auto rawDevice(Context const& context) -> VkDevice
        {
            return static_cast<VkDevice>(static_cast<vk::Device>(*context.device));
        }

        auto rawQueue(Context const& context) -> VkQueue
        {
            return static_cast<VkQueue>(static_cast<vk::Queue>(*context.graphicsQueue));
        }

        auto rawCommandBuffer(vk::raii::CommandBuffer const& commandBuffer) -> VkCommandBuffer
        {
            return static_cast<VkCommandBuffer>(static_cast<vk::CommandBuffer>(*commandBuffer));
        }

        auto imageCount(Swapchain const& swapchain) -> uint32_t
        {
            return std::max(2U, static_cast<uint32_t>(swapchain.images.size()));
        }

        auto checkVkResult(VkResult result) -> void
        {
            if (result != VK_SUCCESS)
            {
                std::cerr << "[ImGui][Vulkan] VkResult " << result << '\n';
            }
        }

    }  // namespace
#endif

    auto GuiPass::init(Window const& window, Context& context, Swapchain const& swapchain) -> bool
    {
#if defined(AERKANIS_IMGUI)
        shutdown();

        try
        {
            if (!context.queueFamilies.graphicsFamily.has_value())
            {
                throw std::runtime_error("graphics queue family is required");
            }

            IMGUI_CHECKVERSION();
            imguiContext = ImGui::CreateContext();
            ImGui::SetCurrentContext(imguiContext);

            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();

            if (!ImGui_ImplGlfw_InitForVulkan(window.nativeWindow, true))
            {
                throw std::runtime_error("GLFW backend init failed");
            }
            glfwBackendInitialized = true;

            colorAttachmentFormat = static_cast<VkFormat>(swapchain.imageFormat);
            pipelineRenderingCreateInfo = VkPipelineRenderingCreateInfoKHR{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &colorAttachmentFormat,
            };

            ImGui_ImplVulkan_InitInfo initInfo{};
            initInfo.ApiVersion = VK_API_VERSION_1_4;
            initInfo.Instance = rawInstance(context);
            initInfo.PhysicalDevice = rawPhysicalDevice(context);
            initInfo.Device = rawDevice(context);
            initInfo.QueueFamily = context.queueFamilies.graphicsFamily.value();
            initInfo.Queue = rawQueue(context);
            initInfo.DescriptorPoolSize = 64;
            initInfo.MinImageCount = imageCount(swapchain);
            initInfo.ImageCount = imageCount(swapchain);
            initInfo.UseDynamicRendering = true;
            initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;
            initInfo.CheckVkResultFn = &checkVkResult;
            initInfo.MinAllocationSize = 1024U * 1024U;

            if (!ImGui_ImplVulkan_Init(&initInfo))
            {
                throw std::runtime_error("Vulkan backend init failed");
            }
            vulkanBackendInitialized = true;
            initialized = true;
            return true;
        }
        catch (std::exception const& exception)
        {
            std::cerr << "[ImGui] init failed: " << exception.what() << '\n';
            shutdown();
            return false;
        }
#else
        (void)window;
        (void)context;
        (void)swapchain;
        return false;
#endif
    }

    auto GuiPass::shutdown() noexcept -> void
    {
#if defined(AERKANIS_IMGUI)
        try
        {
            if (imguiContext != nullptr)
            {
                ImGui::SetCurrentContext(imguiContext);
            }

            if (vulkanBackendInitialized)
            {
                ImGui_ImplVulkan_Shutdown();
            }
            if (glfwBackendInitialized)
            {
                ImGui_ImplGlfw_Shutdown();
            }
            if (imguiContext != nullptr)
            {
                ImGui::DestroyContext(imguiContext);
            }
        }
        catch (...)
        {
        }
#endif

        imguiContext = nullptr;
        colorAttachmentFormat = VK_FORMAT_UNDEFINED;
        pipelineRenderingCreateInfo = {};
        initialized = false;
        glfwBackendInitialized = false;
        vulkanBackendInitialized = false;
    }

    auto GuiPass::beginFrame() -> void
    {
#if defined(AERKANIS_IMGUI)
        if (!initialized || imguiContext == nullptr)
        {
            return;
        }

        ImGui::SetCurrentContext(imguiContext);
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
#endif
    }

    auto GuiPass::wantsMouseCapture() const noexcept -> bool
    {
#if defined(AERKANIS_IMGUI)
        if (!initialized || imguiContext == nullptr)
        {
            return false;
        }

        ImGui::SetCurrentContext(imguiContext);
        return ImGui::GetIO().WantCaptureMouse;
#else
        return false;
#endif
    }

    auto GuiPass::wantsKeyboardCapture() const noexcept -> bool
    {
#if defined(AERKANIS_IMGUI)
        if (!initialized || imguiContext == nullptr)
        {
            return false;
        }

        ImGui::SetCurrentContext(imguiContext);
        return ImGui::GetIO().WantCaptureKeyboard;
#else
        return false;
#endif
    }

    auto GuiPass::drawSceneControls(Scene::SceneState& sceneState) -> void
    {
#if defined(AERKANIS_IMGUI)
        if (initialized && imguiContext != nullptr)
        {
            ImGui::SetCurrentContext(imguiContext);
            ImGui::SetNextWindowPos(ImVec2{16.0F, 16.0F}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2{340.0F, 0.0F}, ImGuiCond_FirstUseEver);

            if (ImGui::Begin("Scene"))
            {
                ImGui::TextUnformatted("Triangle");
                ImGui::SliderFloat("Size", &sceneState.triangle.size, 0.05F, 4.0F, "%.2f");
                ImGui::SliderAngle("Rotation", &sceneState.triangle.rotationRadians, -180.0F, 180.0F);

                ImGui::Separator();
                ImGui::TextUnformatted("Camera");
                ImGui::DragFloat3("Position", &sceneState.camera.position.x, 0.02F, -20.0F, 20.0F, "%.2f");
                ImGui::DragFloat3("Target", &sceneState.camera.target.x, 0.02F, -20.0F, 20.0F, "%.2f");
                ImGui::DragFloat3("Up", &sceneState.camera.up.x, 0.01F, -1.0F, 1.0F, "%.2f");
                ImGui::SliderFloat("FOV", &sceneState.camera.fovYDegrees, 1.0F, 120.0F, "%.1f");
                ImGui::DragFloat("Near", &sceneState.camera.nearPlane, 0.001F, 0.001F, 10.0F, "%.3f");
                ImGui::DragFloat("Far", &sceneState.camera.farPlane, 0.1F, 1.0F, 1000.0F, "%.1f");
            }
            ImGui::End();
        }
#endif

        sceneState.sanitize();
    }

    auto GuiPass::render(vk::raii::CommandBuffer const& commandBuffer) -> void
    {
#if defined(AERKANIS_IMGUI)
        if (!initialized || imguiContext == nullptr)
        {
            return;
        }

        ImGui::SetCurrentContext(imguiContext);
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData != nullptr)
        {
            ImGui_ImplVulkan_RenderDrawData(drawData, rawCommandBuffer(commandBuffer));
        }
#else
        (void)commandBuffer;
#endif
    }

}  // namespace Aerkanis
