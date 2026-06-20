#include "scene/camera.hpp"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace Aerkanis::Scene
{

    auto Camera::sanitize() -> void
    {
        fovYDegrees = std::clamp(fovYDegrees, 1.0F, 120.0F);
        nearPlane = std::max(nearPlane, 0.001F);
        farPlane = std::max(farPlane, nearPlane + 0.001F);

        if (glm::length(target - position) < 0.001F)
        {
            target = position + glm::vec3{0.0F, 0.0F, -1.0F};
        }

        if (glm::length(up) < 0.001F)
        {
            up = glm::vec3{0.0F, 1.0F, 0.0F};
        }

        const glm::vec3 forward = glm::normalize(target - position);
        if (glm::length(glm::cross(forward, glm::normalize(up))) < 0.001F)
        {
            up = glm::vec3{0.0F, 1.0F, 0.0F};
            if (glm::length(glm::cross(forward, up)) < 0.001F)
            {
                up = glm::vec3{1.0F, 0.0F, 0.0F};
            }
        }
    }

    auto Camera::view() const -> glm::mat4
    {
        Camera camera = *this;
        camera.sanitize();
        return glm::lookAt(camera.position, camera.target, camera.up);
    }

    auto Camera::projection(float aspectRatio) const -> glm::mat4
    {
        Camera camera = *this;
        camera.sanitize();

        const float safeAspectRatio = std::max(aspectRatio, 0.001F);
        glm::mat4 result = glm::perspective(
            glm::radians(camera.fovYDegrees),
            safeAspectRatio,
            camera.nearPlane,
            camera.farPlane);
        result[1][1] *= -1.0F;
        return result;
    }

    auto Camera::viewProjection(float aspectRatio) const -> glm::mat4
    {
        return projection(aspectRatio) * view();
    }

}  // namespace Aerkanis::Scene
