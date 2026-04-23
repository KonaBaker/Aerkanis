#include "app/window.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Detail {

auto glfwRefCount() -> int& {
    static int refCount = 0;
    return refCount;
}

auto glfwErrorCallback(int error, const char* description) -> void {
    std::cerr << "[GLFW] Error " << error << ": "
              << (description != nullptr ? description : "unknown") << '\n';
}

auto ensureGlfwInitialized() -> bool {
    int& refCount = glfwRefCount();
    if (refCount == 0) {
        glfwSetErrorCallback(glfwErrorCallback);
        if (glfwInit() == GLFW_FALSE) {
            return false;
        }
    }
    ++refCount;
    return true;
}

auto releaseGlfw() noexcept -> void {
    int& refCount = glfwRefCount();
    if (refCount <= 0) {
        return;
    }

    --refCount;
    if (refCount == 0) {
        glfwTerminate();
    }
}

auto windowFromHandle(GLFWwindow* nextWindow) noexcept -> Aerkanis::Window* {
    if (nextWindow == nullptr) {
        return nullptr;
    }

    return static_cast<Aerkanis::Window*>(glfwGetWindowUserPointer(nextWindow));
}

}  // namespace Detail

namespace Aerkanis {

auto Window::init(int width, int height, std::string title, bool resizable) -> bool {
    if (!Detail::ensureGlfwInitialized()) {
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);

    const int safeWidth = width > 0 ? width : 1;
    const int safeHeight = height > 0 ? height : 1;
    nativeWindow = glfwCreateWindow(safeWidth, safeHeight, title.c_str(), nullptr, nullptr);
    if (nativeWindow == nullptr) {
        Detail::releaseGlfw();
        return false;
    }

    glfwSetWindowUserPointer(nativeWindow, this);
    glfwSetFramebufferSizeCallback(nativeWindow, &Window::onFramebufferSize);
    glfwSetWindowCloseCallback(nativeWindow, &Window::onWindowClose);
    glfwSetKeyCallback(nativeWindow, &Window::onKey);
    glfwSetCursorPosCallback(nativeWindow, &Window::onCursor);
    return true;
}

auto Window::shutdown() noexcept -> void {
    if (nativeWindow != nullptr) {
        glfwDestroyWindow(nativeWindow);
        nativeWindow = nullptr;
    }

    Detail::releaseGlfw();
}

auto Window::handle() const noexcept -> GLFWwindow* {
    return nativeWindow;
}

auto Window::shouldClose() const noexcept -> bool {
    return nativeWindow != nullptr && glfwWindowShouldClose(nativeWindow) == GLFW_TRUE;
}

auto Window::framebufferSize() const noexcept -> std::pair<int, int> {
    int width = 0;
    int height = 0;
    if (nativeWindow != nullptr) {
        glfwGetFramebufferSize(nativeWindow, &width, &height);
    }
    return {width, height};
}

auto Window::windowSize() const noexcept -> std::pair<int, int> {
    int width = 0;
    int height = 0;
    if (nativeWindow != nullptr) {
        glfwGetWindowSize(nativeWindow, &width, &height);
    }
    return {width, height};
}

auto Window::createSurface(const vk::raii::Instance& instance) const -> vk::raii::SurfaceKHR {
    if (nativeWindow == nullptr) {
        throw std::runtime_error("Cannot create a Vulkan surface from a null window");
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const VkInstance rawInstance = static_cast<VkInstance>(*instance);
    const VkResult result = glfwCreateWindowSurface(rawInstance, nativeWindow, nullptr, &surface);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            "glfwCreateWindowSurface failed with VkResult " +
            std::to_string(static_cast<int>(result)));
    }

    return vk::raii::SurfaceKHR(instance, surface);
}

auto Window::pollEvents() const noexcept -> void {
    glfwPollEvents();
}

auto Window::setTitle(const std::string& nextTitle) -> void {
    if (nativeWindow != nullptr) {
        glfwSetWindowTitle(nativeWindow, nextTitle.c_str());
    }
}

auto Window::setResizeCallback(ResizeCallback callback) -> void {
    resizeCallback = std::move(callback);
}

auto Window::setCloseCallback(CloseCallback callback) -> void {
    closeCallback = std::move(callback);
}

auto Window::setKeyCallback(KeyCallback callback) -> void {
    keyCallback = std::move(callback);
}

auto Window::setCursorCallback(CursorCallback callback) -> void {
    cursorCallback = std::move(callback);
}

auto Window::onFramebufferSize(GLFWwindow* nextWindow, int width, int height) -> void {
    if (auto* self = Detail::windowFromHandle(nextWindow); self != nullptr && self->resizeCallback) {
        self->resizeCallback(width, height);
    }
}

auto Window::onWindowClose(GLFWwindow* nextWindow) -> void {
    if (auto* self = Detail::windowFromHandle(nextWindow); self != nullptr && self->closeCallback) {
        self->closeCallback();
    }
}

auto Window::onKey(GLFWwindow* nextWindow, int key, int scancode, int action, int mods) -> void {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(nextWindow, GLFW_TRUE);
    }

    if (auto* self = Detail::windowFromHandle(nextWindow); self != nullptr && self->keyCallback) {
        self->keyCallback(key, scancode, action, mods);
    }
}

auto Window::onCursor(GLFWwindow* nextWindow, double xpos, double ypos) -> void {
    if (auto* self = Detail::windowFromHandle(nextWindow); self != nullptr && self->cursorCallback) {
        self->cursorCallback(xpos, ypos);
    }
}

}  // namespace Aerkanis
