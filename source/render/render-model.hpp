#pragma once

#include <algorithm>

#include "source-cloud/cloud-model.hpp"

namespace Aerkanis
{

    enum struct RenderPipeline : int
    {
        TriangleDemo = 0,
        SunCloudNubis = 1,
        SunCloudNubisCubed = 2,
    };

    struct RenderSettings
    {
        RenderPipeline pipeline{RenderPipeline::SunCloudNubis};
        Cloud::CloudNubisCubedSettings nubisCubed{};

        auto sanitize() -> void
        {
            if (pipeline != RenderPipeline::TriangleDemo &&
                pipeline != RenderPipeline::SunCloudNubis &&
                pipeline != RenderPipeline::SunCloudNubisCubed)
            {
                pipeline = RenderPipeline::SunCloudNubis;
            }
            nubisCubed.sanitize();
        }
    };

    auto renderPipelineName(RenderPipeline pipeline) noexcept -> const char*;

}  // namespace Aerkanis
