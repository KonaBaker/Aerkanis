#include "render/render-model.hpp"

namespace Aerkanis
{

    auto renderPipelineName(RenderPipeline pipeline) noexcept -> const char*
    {
        switch (pipeline)
        {
        case RenderPipeline::TriangleDemo:
            return "Triangle Demo";
        case RenderPipeline::SunCloudNubis:
            return "Sun Cloud Nubis";
        case RenderPipeline::SunCloudNubisCubed:
            return "Nubis3 Reserved";
        }

        return "Sun Cloud Nubis";
    }

}  // namespace Aerkanis
