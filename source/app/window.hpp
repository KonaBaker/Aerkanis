#pragma once

#include <string>

struct GLFWwindow;

namespace Aerkanis
{

    struct Window
    {
        GLFWwindow* nativeWindow{};

        auto init(int width, int height, std::string title, bool resizable = true) -> bool;
        auto shutdown() noexcept -> void;
        auto shouldClose() const noexcept -> bool;
        auto getFramebufferSize(int& width, int& height) const noexcept -> void;
    };

}  // namespace Aerkanis
