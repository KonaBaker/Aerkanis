#include "scene/camera-controller.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include "app/window.hpp"
#include "scene/camera.hpp"

namespace Aerkanis::Scene
{

    namespace
    {

        auto keyDown(GLFWwindow* window, int key) -> bool
        {
            return glfwGetKey(window, key) == GLFW_PRESS;
        }

        auto mouseButtonDown(GLFWwindow* window, int button) -> bool
        {
            return glfwGetMouseButton(window, button) == GLFW_PRESS;
        }

        auto cameraDirection(float yawRadians, float pitchRadians) -> glm::vec3
        {
            return glm::normalize(glm::vec3{
                std::cos(yawRadians) * std::cos(pitchRadians),
                std::sin(pitchRadians),
                std::sin(yawRadians) * std::cos(pitchRadians),
            });
        }

        auto releaseCursor(GLFWwindow* window) -> void
        {
            if (glfwRawMouseMotionSupported() == GLFW_TRUE)
            {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            }
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        auto captureCursor(GLFWwindow* window) -> void
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            if (glfwRawMouseMotionSupported() == GLFW_TRUE)
            {
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
        }

    }  // namespace

    auto CameraController::syncFromCamera(Camera const& camera) -> void
    {
        glm::vec3 direction = camera.target - camera.position;
        if (glm::length(direction) < 0.001F)
        {
            direction = glm::vec3{0.0F, 0.0F, -1.0F};
        }

        direction = glm::normalize(direction);
        yawRadians = std::atan2(direction.z, direction.x);
        pitchRadians = std::asin(std::clamp(direction.y, -1.0F, 1.0F));
    }

    auto CameraController::update(
        Window const& window,
        Camera& camera,
        float deltaSeconds,
        bool guiWantsMouse,
        bool guiWantsKeyboard) -> void
    {
        if (window.nativeWindow == nullptr)
        {
            return;
        }

        GLFWwindow* nativeWindow = window.nativeWindow;
        if (!cursorCaptured)
        {
            syncFromCamera(camera);
        }

        const bool wantsLook = mouseButtonDown(nativeWindow, GLFW_MOUSE_BUTTON_RIGHT);
        const bool canStartLook = wantsLook && (!guiWantsMouse || cursorCaptured);

        if (canStartLook && !cursorCaptured)
        {
            captureCursor(nativeWindow);
            glfwGetCursorPos(nativeWindow, &previousCursorX, &previousCursorY);
            cursorCaptured = true;
            cursorInitialized = true;
        }
        else if (!wantsLook && cursorCaptured)
        {
            releaseCursor(nativeWindow);
            cursorCaptured = false;
            cursorInitialized = false;
        }

        if (cursorCaptured)
        {
            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetCursorPos(nativeWindow, &cursorX, &cursorY);

            if (cursorInitialized)
            {
                const double deltaX = cursorX - previousCursorX;
                const double deltaY = cursorY - previousCursorY;
                yawRadians += static_cast<float>(deltaX) * mouseSensitivity;
                pitchRadians -= static_cast<float>(deltaY) * mouseSensitivity;
            }

            previousCursorX = cursorX;
            previousCursorY = cursorY;
            cursorInitialized = true;
        }

        constexpr float maxPitch = std::numbers::pi_v<float> * 0.49F;
        pitchRadians = std::clamp(pitchRadians, -maxPitch, maxPitch);

        const glm::vec3 forward = cameraDirection(yawRadians, pitchRadians);
        const glm::vec3 worldUp{0.0F, 1.0F, 0.0F};
        const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));

        const bool canMove = cursorCaptured || !guiWantsKeyboard;
        glm::vec3 movement{0.0F};
        if (canMove)
        {
            if (keyDown(nativeWindow, GLFW_KEY_W))
            {
                movement += forward;
            }
            if (keyDown(nativeWindow, GLFW_KEY_S))
            {
                movement -= forward;
            }
            if (keyDown(nativeWindow, GLFW_KEY_D))
            {
                movement += right;
            }
            if (keyDown(nativeWindow, GLFW_KEY_A))
            {
                movement -= right;
            }
            if (keyDown(nativeWindow, GLFW_KEY_E))
            {
                movement += up;
            }
            if (keyDown(nativeWindow, GLFW_KEY_Q))
            {
                movement -= up;
            }
        }

        if (glm::length(movement) > 0.001F)
        {
            movement = glm::normalize(movement);
            float speed = movementSpeed;
            if (keyDown(nativeWindow, GLFW_KEY_LEFT_SHIFT) || keyDown(nativeWindow, GLFW_KEY_RIGHT_SHIFT))
            {
                speed *= sprintMultiplier;
            }
            camera.position += movement * speed * std::max(deltaSeconds, 0.0F);
        }

        camera.target = camera.position + forward;
        camera.up = up;
        camera.sanitize();
    }

}  // namespace Aerkanis::Scene
