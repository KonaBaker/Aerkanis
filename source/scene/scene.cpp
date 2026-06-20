#include "scene/scene.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Aerkanis::Scene
{

    auto SceneState::sanitize() -> void
    {
        if (!std::isfinite(triangle.size))
        {
            triangle.size = 1.0F;
        }
        if (!std::isfinite(triangle.rotationRadians))
        {
            triangle.rotationRadians = 0.0F;
        }

        triangle.size = std::clamp(triangle.size, 0.05F, 4.0F);

        constexpr float fullTurn = std::numbers::pi_v<float> * 2.0F;
        triangle.rotationRadians = std::fmod(triangle.rotationRadians, fullTurn);

        camera.sanitize();
    }

}  // namespace Aerkanis::Scene
