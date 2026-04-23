#pragma once

#include <functional>
#include <string>
#include <utility>

#include <vulkan/vulkan_raii.hpp>

struct GLFWwindow;

namespace Aerkanis
{

    struct Window
    {
        using ResizeCallback = std::function<void(int, int)>;
        using CloseCallback = std::function<void()>;
        using KeyCallback = std::function<void(int, int, int, int)>;
        using CursorCallback = std::function<void(double, double)>;

        GLFWwindow* nativeWindow{};
        ResizeCallback resizeCallback{};
        CloseCallback closeCallback{};
        KeyCallback keyCallback{};
        CursorCallback cursorCallback{};

        auto init(int width, int height, std::string title, bool resizable = true) -> bool;
        auto shutdown() noexcept -> void;
        auto handle() const noexcept -> GLFWwindow*;
        auto shouldClose() const noexcept -> bool;
        auto framebufferSize() const noexcept -> std::pair<int, int>;
        auto windowSize() const noexcept -> std::pair<int, int>;
        auto createSurface(const vk::raii::Instance& instance) const -> vk::raii::SurfaceKHR;
        auto pollEvents() const noexcept -> void;
        auto setTitle(const std::string& nextTitle) -> void;
        auto setResizeCallback(ResizeCallback callback) -> void;
        auto setCloseCallback(CloseCallback callback) -> void;
        auto setKeyCallback(KeyCallback callback) -> void;
        auto setCursorCallback(CursorCallback callback) -> void;

        static auto onFramebufferSize(GLFWwindow* nextWindow, int width, int height) -> void;
        static auto onWindowClose(GLFWwindow* nextWindow) -> void;
        static auto onKey(GLFWwindow* nextWindow, int key, int scancode, int action, int mods) -> void;
        static auto onCursor(GLFWwindow* nextWindow, double xpos, double ypos) -> void;
    };

}  // namespace Aerkanis
