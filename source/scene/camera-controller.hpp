#pragma once

namespace Aerkanis
{

    struct Window;

}  // namespace Aerkanis

namespace Aerkanis::Scene
{

    struct Camera;

    struct CameraController
    {
        float movementSpeed{2.5F};
        float sprintMultiplier{3.0F};
        float mouseSensitivity{0.0025F};
        float yawRadians{0.0F};
        float pitchRadians{0.0F};
        double previousCursorX{0.0};
        double previousCursorY{0.0};
        bool cursorCaptured{false};
        bool cursorInitialized{false};

        auto syncFromCamera(Camera const& camera) -> void;
        auto update(
            Window const& window,
            Camera& camera,
            float deltaSeconds,
            bool guiWantsMouse,
            bool guiWantsKeyboard) -> void;
    };

}  // namespace Aerkanis::Scene
