#include "app/window.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Details
{

    auto glfwRefCount() -> int&
    {
        static int refCount = 0;
        return refCount;
    }

    auto glfwErrorCallback(int error, const char* description) -> void
    {
        std::cerr << "[GLFW] Error " << error << ": "
                << (description != nullptr ? description : "unknown") << '\n';
    }

    auto ensureGlfwInitialized() -> bool
    {
        int& refCount = glfwRefCount();
        if (refCount == 0)
        {
            glfwSetErrorCallback(glfwErrorCallback);
            if (glfwInit() == GLFW_FALSE)
            {
                return false;
            }
        }
        ++refCount;
        return true;
    }

    auto releaseGlfw() noexcept -> void
    {
        int& refCount = glfwRefCount();
        if (refCount <= 0)
        {
            return;
        }

        --refCount;
        if (refCount == 0)
        {
            glfwTerminate();
        }
    }

    auto windowFromHandle(GLFWwindow* nextWindow) noexcept -> Aerkanis::Window*
    {
        if (nextWindow == nullptr)
        {
            return nullptr;
        }

        return static_cast<Aerkanis::Window*>(glfwGetWindowUserPointer(nextWindow));
    }

}  // namespace Details

namespace Aerkanis
{

    auto Window::init(int width, int height, std::string title, bool resizable) -> bool
    {
        if (!Details::ensureGlfwInitialized())
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
            Details::releaseGlfw();
            return false;
        }

        glfwSetWindowUserPointer(nativeWindow, this);
        glfwSetFramebufferSizeCallback(nativeWindow, &Window::onFramebufferSize);
        return true;
    }

    auto Window::shutdown() noexcept -> void
    {
        if (nativeWindow != nullptr)
        {
            glfwDestroyWindow(nativeWindow);
            nativeWindow = nullptr;
        }

        resizeCallback = {};
        Details::releaseGlfw();
    }

    auto Window::shouldClose() const noexcept -> bool
    {
        return nativeWindow != nullptr && glfwWindowShouldClose(nativeWindow) == GLFW_TRUE;
    }

    auto Window::setResizeCallback(ResizeCallback callback) -> void
    {
        resizeCallback = std::move(callback);
    }

    auto Window::getFramebufferSize(int& width, int& height) const noexcept -> void
    {
        glfwGetFramebufferSize(nativeWindow, &width, &height);
    }

    auto Window::onFramebufferSize(GLFWwindow* nextWindow, int width, int height) -> void
    {
        if (auto* self = Details::windowFromHandle(nextWindow); self != nullptr && self->resizeCallback)
        {
            self->resizeCallback(width, height);
        }
    }

}  // namespace Aerkanis
