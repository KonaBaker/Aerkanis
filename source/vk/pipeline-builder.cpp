#include "vk/pipeline-builder.hpp"

#include <array>
#include <stdexcept>

namespace Aerkanis::Vk
{

    namespace
    {

        auto validateColorFormats(std::span<const vk::Format> formats) -> void
        {
            for (vk::Format format : formats)
            {
                if (format == vk::Format::eUndefined)
                {
                    throw std::runtime_error("Pipeline color attachment format cannot be undefined");
                }
            }
        }

        auto validateColorBlendIndex(PipelineBuilder const& builder, uint32_t index) -> void
        {
            if (index >= builder.colorBlendAttachments.size())
            {
                throw std::runtime_error("Pipeline color blend attachment index is out of range");
            }
        }

        auto isComputeStage(vk::PipelineShaderStageCreateInfo const& stage) -> bool
        {
            return stage.stage == vk::ShaderStageFlagBits::eCompute;
        }

        auto validateGraphicsState(PipelineBuilder const& builder) -> void
        {
            if (builder.shaderStages.empty())
            {
                throw std::runtime_error("Graphics pipeline requires at least one shader stage");
            }

            for (vk::PipelineShaderStageCreateInfo const& stage : builder.shaderStages)
            {
                if (isComputeStage(stage))
                {
                    throw std::runtime_error("Graphics pipeline cannot use a compute shader stage");
                }
            }

            if (builder.colorFormats.empty() &&
                builder.depthFormat == vk::Format::eUndefined &&
                builder.stencilFormat == vk::Format::eUndefined)
            {
                throw std::runtime_error("Graphics pipeline requires at least one rendering attachment format");
            }

            if (builder.colorFormats.size() != builder.colorBlendAttachments.size())
            {
                throw std::runtime_error("Graphics pipeline color format and blend attachment counts differ");
            }
        }

        auto findComputeShaderStage(PipelineBuilder const& builder) -> vk::PipelineShaderStageCreateInfo
        {
            vk::PipelineShaderStageCreateInfo computeStage{};
            bool found = false;

            for (vk::PipelineShaderStageCreateInfo const& stage : builder.shaderStages)
            {
                if (!isComputeStage(stage))
                {
                    continue;
                }

                if (found)
                {
                    throw std::runtime_error("Compute pipeline requires exactly one compute shader stage");
                }

                computeStage = stage;
                found = true;
            }

            if (!found)
            {
                throw std::runtime_error("Compute pipeline requires a compute shader stage");
            }

            return computeStage;
        }

    }  // namespace

    auto defaultColorBlendAttachment() noexcept -> vk::PipelineColorBlendAttachmentState
    {
        return vk::PipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .colorWriteMask =
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA,
        };
    }

    auto PipelineBuilder::reset() -> PipelineBuilder&
    {
        *this = PipelineBuilder{};
        return *this;
    }

    auto PipelineBuilder::addShaderStage(vk::PipelineShaderStageCreateInfo shaderStage) -> PipelineBuilder&
    {
        shaderStages.push_back(shaderStage);
        return *this;
    }

    auto PipelineBuilder::addShaderStage(
        ShaderModule const& shaderModule,
        vk::ShaderStageFlagBits shaderStage,
        const char* entryPoint) -> PipelineBuilder&
    {
        return addShaderStage(shaderModule.stage(shaderStage, entryPoint));
    }

    auto PipelineBuilder::clearShaderStages() -> PipelineBuilder&
    {
        shaderStages.clear();
        return *this;
    }

    auto PipelineBuilder::setVertexInput(
        std::span<const vk::VertexInputBindingDescription> bindings,
        std::span<const vk::VertexInputAttributeDescription> attributes) -> PipelineBuilder&
    {
        vertexBindings.assign(bindings.begin(), bindings.end());
        vertexAttributes.assign(attributes.begin(), attributes.end());
        return *this;
    }

    auto PipelineBuilder::setInputTopology(vk::PrimitiveTopology topology, bool primitiveRestart)
        -> PipelineBuilder&
    {
        inputAssembly.topology = topology;
        inputAssembly.primitiveRestartEnable = primitiveRestart ? VK_TRUE : VK_FALSE;
        return *this;
    }

    auto PipelineBuilder::setPolygonMode(vk::PolygonMode polygonMode, float lineWidth) -> PipelineBuilder&
    {
        rasterizer.polygonMode = polygonMode;
        rasterizer.lineWidth = lineWidth;
        return *this;
    }

    auto PipelineBuilder::setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace) -> PipelineBuilder&
    {
        rasterizer.cullMode = cullMode;
        rasterizer.frontFace = frontFace;
        return *this;
    }

    auto PipelineBuilder::setColorAttachmentFormat(vk::Format format) -> PipelineBuilder&
    {
        const std::array formats{format};
        return setColorAttachmentFormats(formats);
    }

    auto PipelineBuilder::setColorAttachmentFormats(std::span<const vk::Format> formats) -> PipelineBuilder&
    {
        validateColorFormats(formats);
        colorFormats.assign(formats.begin(), formats.end());
        colorBlendAttachments.assign(colorFormats.size(), defaultColorBlendAttachment());
        return *this;
    }

    auto PipelineBuilder::disableColorAttachments() -> PipelineBuilder&
    {
        colorFormats.clear();
        colorBlendAttachments.clear();
        return *this;
    }

    auto PipelineBuilder::setColorBlendAttachment(
        uint32_t index,
        vk::PipelineColorBlendAttachmentState attachment) -> PipelineBuilder&
    {
        validateColorBlendIndex(*this, index);
        colorBlendAttachments[index] = attachment;
        return *this;
    }

    auto PipelineBuilder::enableAlphaBlending(uint32_t index) -> PipelineBuilder&
    {
        validateColorBlendIndex(*this, index);

        vk::PipelineColorBlendAttachmentState attachment = defaultColorBlendAttachment();
        attachment.blendEnable = VK_TRUE;
        attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        attachment.colorBlendOp = vk::BlendOp::eAdd;
        attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        attachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        attachment.alphaBlendOp = vk::BlendOp::eAdd;
        colorBlendAttachments[index] = attachment;
        return *this;
    }

    auto PipelineBuilder::disableBlending(uint32_t index) -> PipelineBuilder&
    {
        validateColorBlendIndex(*this, index);
        colorBlendAttachments[index] = defaultColorBlendAttachment();
        return *this;
    }

    auto PipelineBuilder::enableDepth(
        vk::Format format,
        bool writeDepth,
        vk::CompareOp compareOp) -> PipelineBuilder&
    {
        if (format == vk::Format::eUndefined)
        {
            throw std::runtime_error("Depth format cannot be undefined");
        }

        depthFormat = format;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = writeDepth ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = compareOp;
        return *this;
    }

    auto PipelineBuilder::disableDepth() -> PipelineBuilder&
    {
        depthFormat = vk::Format::eUndefined;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
        return *this;
    }

    auto PipelineBuilder::setStencilFormat(vk::Format format) -> PipelineBuilder&
    {
        stencilFormat = format;
        depthStencil.stencilTestEnable = format == vk::Format::eUndefined ? VK_FALSE : VK_TRUE;
        return *this;
    }

    auto PipelineBuilder::setDynamicStates(std::span<const vk::DynamicState> states) -> PipelineBuilder&
    {
        dynamicStates.assign(states.begin(), states.end());
        return *this;
    }

    auto PipelineBuilder::addDescriptorSetLayout(vk::DescriptorSetLayout layout) -> PipelineBuilder&
    {
        if (layout == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Pipeline descriptor set layout cannot be null");
        }

        descriptorSetLayouts.push_back(layout);
        return *this;
    }

    auto PipelineBuilder::addDescriptorSetLayout(DescriptorSetLayout const& layout) -> PipelineBuilder&
    {
        return addDescriptorSetLayout(layout.handle());
    }

    auto PipelineBuilder::setDescriptorSetLayouts(std::span<const vk::DescriptorSetLayout> layouts)
        -> PipelineBuilder&
    {
        descriptorSetLayouts.clear();
        descriptorSetLayouts.reserve(layouts.size());
        for (vk::DescriptorSetLayout layout : layouts)
        {
            addDescriptorSetLayout(layout);
        }
        return *this;
    }

    auto PipelineBuilder::addPushConstantRange(vk::PushConstantRange range) -> PipelineBuilder&
    {
        if (range.size == 0)
        {
            throw std::runtime_error("Pipeline push constant range requires a non-zero size");
        }

        pushConstantRanges.push_back(range);
        return *this;
    }

    auto PipelineBuilder::setPushConstantRanges(std::span<const vk::PushConstantRange> ranges)
        -> PipelineBuilder&
    {
        pushConstantRanges.clear();
        for (vk::PushConstantRange range : ranges)
        {
            addPushConstantRange(range);
        }
        return *this;
    }

    auto PipelineBuilder::buildPipelineLayout(vk::raii::Device const& device) const
        -> vk::raii::PipelineLayout
    {
        vk::PipelineLayoutCreateInfo layoutCreateInfo{
            .flags = layoutFlags,
            .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
            .pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data(),
        };

        return vk::raii::PipelineLayout(device, layoutCreateInfo);
    }

    auto PipelineBuilder::buildGraphicsPipeline(vk::raii::Device const& device, vk::PipelineLayout layout) const
        -> vk::raii::Pipeline
    {
        validateGraphicsState(*this);

        vk::PipelineVertexInputStateCreateInfo vertexInputState{
            .vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindings.size()),
            .pVertexBindingDescriptions = vertexBindings.empty() ? nullptr : vertexBindings.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
            .pVertexAttributeDescriptions = vertexAttributes.empty() ? nullptr : vertexAttributes.data(),
        };

        vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .scissorCount = 1,
        };

        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.empty() ? nullptr : dynamicStates.data(),
        };

        vk::PipelineColorBlendStateCreateInfo colorBlendState{
            .logicOpEnable = VK_FALSE,
            .attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
            .pAttachments = colorBlendAttachments.empty() ? nullptr : colorBlendAttachments.data(),
        };

        vk::PipelineRenderingCreateInfo renderingInfo{
            .colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
            .pColorAttachmentFormats = colorFormats.empty() ? nullptr : colorFormats.data(),
            .depthAttachmentFormat = depthFormat,
            .stencilAttachmentFormat = stencilFormat,
        };

        vk::GraphicsPipelineCreateInfo pipelineCreateInfo{
            .pNext = &renderingInfo,
            .flags = pipelineFlags,
            .stageCount = static_cast<uint32_t>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputState,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlendState,
            .pDynamicState = dynamicStates.empty() ? nullptr : &dynamicState,
            .layout = layout,
        };

        return vk::raii::Pipeline(device, nullptr, pipelineCreateInfo);
    }

    auto PipelineBuilder::buildGraphics(vk::raii::Device const& device) const -> PipelineBuildResult
    {
        PipelineBuildResult result{};
        result.layout = buildPipelineLayout(device);
        result.pipeline = buildGraphicsPipeline(device, static_cast<vk::PipelineLayout>(*result.layout));
        return result;
    }

    auto PipelineBuilder::buildComputePipeline(vk::raii::Device const& device, vk::PipelineLayout layout) const
        -> vk::raii::Pipeline
    {
        const vk::PipelineShaderStageCreateInfo computeStage = findComputeShaderStage(*this);
        vk::ComputePipelineCreateInfo pipelineCreateInfo{
            .flags = pipelineFlags,
            .stage = computeStage,
            .layout = layout,
        };

        return vk::raii::Pipeline(device, nullptr, pipelineCreateInfo);
    }

    auto PipelineBuilder::buildCompute(vk::raii::Device const& device) const -> PipelineBuildResult
    {
        PipelineBuildResult result{};
        result.layout = buildPipelineLayout(device);
        result.pipeline = buildComputePipeline(device, static_cast<vk::PipelineLayout>(*result.layout));
        return result;
    }

}  // namespace Aerkanis::Vk
