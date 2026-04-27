#pragma once

#include <string>

#include "app/window.hpp"

struct GLFWwindow;

namespace Aerkanis
{

    struct ApplicationConfig
    {
        int width = 1280;
        int height = 720;
        std::string title = "Aerkanis";
        bool resizable = true;
    };

    struct Application
    {
        ApplicationConfig config;
        Window appWindow;
        bool running = false;
        bool framebufferResized = false;

        auto init(ApplicationConfig nextConfig = {}) -> bool;
        auto window() noexcept -> Window&;
        auto window() const noexcept -> const Window&;
        auto requestQuit() noexcept -> void;
        auto shutdown() noexcept -> void;
        auto run() -> int;

        static void framebufferResizedCallback(GLFWwindow* window, int width, int height);
    };

}  // namespace Aerkanis
