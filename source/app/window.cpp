#include "app/window.hpp"

#include <iostream>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "app/application.hpp"

namespace
{
    bool glfwInitialized = false;

    auto glfwErrorCallback(int error, const char* description) -> void
    {
        std::cerr << "[GLFW] Error " << error << ": "
                << (description != nullptr ? description : "unknown") << '\n';
    }

    auto ensureGlfwInitialized() -> bool
    {
        if (!glfwInitialized)
        {
            glfwSetErrorCallback(glfwErrorCallback);
            if (glfwInit() == GLFW_FALSE)
            {
                return false;
            }
            glfwInitialized = true;
        }
        return true;
    }

    auto releaseGlfw() noexcept -> void
    {
        if (!glfwInitialized)
        {
            return;
        }

        glfwTerminate();
        glfwInitialized = false;
    }


}  // namespace

namespace Aerkanis
{

    auto Window::init(int width, int height, std::string title, bool resizable) -> bool
    {
        if (!ensureGlfwInitialized())
        {
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);

        const int safeWidth = width > 0 ? width : 1;
        const int safeHeight = height > 0 ? height : 1;
        nativeWindow = glfwCreateWindow(safeWidth, safeHeight, title.c_str(), nullptr, nullptr);
        if (nativeWindow == nullptr)
        {
            releaseGlfw();
            return false;
        }

        glfwSetWindowUserPointer(nativeWindow, this);
        glfwSetFramebufferSizeCallback(nativeWindow, &Application::framebufferResizedCallback);
        return true;
    }

    auto Window::shutdown() noexcept -> void
    {
        if (nativeWindow != nullptr)
        {
            glfwDestroyWindow(nativeWindow);
            nativeWindow = nullptr;
        }

        releaseGlfw();
    }

    auto Window::shouldClose() const noexcept -> bool
    {
        return nativeWindow != nullptr && glfwWindowShouldClose(nativeWindow) == GLFW_TRUE;
    }

    auto Window::getFramebufferSize(int& width, int& height) const noexcept-> void
    {
        glfwGetFramebufferSize(nativeWindow, &width, &height);
    }

}  // namespace Aerkanis
