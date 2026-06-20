#pragma once

#include <glm/glm.hpp>

namespace Aerkanis::Scene
{

    struct Camera
    {
        glm::vec3 position{0.0F, 0.0F, 2.6F};
        glm::vec3 target{0.0F, 0.0F, 0.0F};
        glm::vec3 up{0.0F, 1.0F, 0.0F};
        float fovYDegrees{45.0F};
        float nearPlane{0.01F};
        float farPlane{100.0F};

        auto sanitize() -> void;
        auto view() const -> glm::mat4;
        auto projection(float aspectRatio) const -> glm::mat4;
        auto viewProjection(float aspectRatio) const -> glm::mat4;
    };

}  // namespace Aerkanis::Scene
