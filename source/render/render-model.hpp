#pragma once

#include <algorithm>

namespace Aerkanis
{

    enum struct RenderPipeline : int
    {
        TriangleDemo = 0,
        SunCloudNubis = 1,
        SunCloudNubisCubed = 2,
    };

    struct NubisCubedPipelineSettings
    {
        bool enabled{false};
        int datasetIndex{0};
        float voxelScale{1.0F};
        float densityMultiplier{1.0F};

        auto sanitize() -> void
        {
            datasetIndex = std::clamp(datasetIndex, 0, 16);
            voxelScale = std::clamp(voxelScale, 0.01F, 64.0F);
            densityMultiplier = std::clamp(densityMultiplier, 0.0F, 8.0F);
        }
    };

    struct RenderSettings
    {
        RenderPipeline pipeline{RenderPipeline::SunCloudNubis};
        NubisCubedPipelineSettings nubisCubed{};

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
