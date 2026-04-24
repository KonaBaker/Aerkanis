#pragma once

#include <functional>
#include <string>

struct GLFWwindow;

namespace Aerkanis
{

    struct Window
    {
        using ResizeCallback = std::function<void(int, int)>;

        GLFWwindow* nativeWindow{};
        ResizeCallback resizeCallback{};

        auto init(int width, int height, std::string title, bool resizable = true) -> bool;
        auto shutdown() noexcept -> void;
        auto shouldClose() const noexcept -> bool;
        auto setResizeCallback(ResizeCallback callback) -> void;

        static auto onFramebufferSize(GLFWwindow* nextWindow, int width, int height) -> void;
    };

}  // namespace Aerkanis
