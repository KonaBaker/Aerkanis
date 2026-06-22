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
            return "Sun Cloud Nubis3";
        }

        return "Sun Cloud Nubis";
    }

}  // namespace Aerkanis
