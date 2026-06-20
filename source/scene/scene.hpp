#pragma once

#include "scene/camera.hpp"

namespace Aerkanis::Scene
{

    struct TriangleObject
    {
        float size{1.0F};
        float rotationRadians{0.0F};
    };

    struct SceneState
    {
        Camera camera{};
        TriangleObject triangle{};

        auto sanitize() -> void;
    };

}  // namespace Aerkanis::Scene
