#pragma once

#include <span>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "vk/descriptor-set-layout.hpp"
#include "vk/shader.hpp"

namespace Aerkanis::Vk
{

    auto defaultColorBlendAttachment() noexcept -> vk::PipelineColorBlendAttachmentState;

    struct PipelineBuildResult
    {
        vk::raii::PipelineLayout layout{nullptr};
        vk::raii::Pipeline pipeline{nullptr};
    };

    struct PipelineBuilder
    {
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{};
        std::vector<vk::VertexInputBindingDescription> vertexBindings{};
        std::vector<vk::VertexInputAttributeDescription> vertexAttributes{};
        std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments{defaultColorBlendAttachment()};
        std::vector<vk::DynamicState> dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        std::vector<vk::Format> colorFormats{};
        std::vector<vk::DescriptorSetLayout> descriptorSetLayouts{};
        std::vector<vk::PushConstantRange> pushConstantRanges{};
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE,
        };
        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0F,
        };
        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE,
        };
        vk::PipelineDepthStencilStateCreateInfo depthStencil{
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = vk::CompareOp::eLessOrEqual,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
        };
        vk::Format depthFormat{vk::Format::eUndefined};
        vk::Format stencilFormat{vk::Format::eUndefined};
        vk::PipelineLayoutCreateFlags layoutFlags{};
        vk::PipelineCreateFlags pipelineFlags{};

        auto reset() -> PipelineBuilder&;
        auto addShaderStage(vk::PipelineShaderStageCreateInfo shaderStage) -> PipelineBuilder&;
        auto addShaderStage(
            ShaderModule const& shaderModule,
            vk::ShaderStageFlagBits shaderStage,
            const char* entryPoint) -> PipelineBuilder&;
        auto clearShaderStages() -> PipelineBuilder&;
        auto setVertexInput(
            std::span<const vk::VertexInputBindingDescription> bindings,
            std::span<const vk::VertexInputAttributeDescription> attributes) -> PipelineBuilder&;
        auto setInputTopology(vk::PrimitiveTopology topology, bool primitiveRestart = false)
            -> PipelineBuilder&;
        auto setPolygonMode(vk::PolygonMode polygonMode, float lineWidth = 1.0F) -> PipelineBuilder&;
        auto setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace) -> PipelineBuilder&;
        auto setColorAttachmentFormat(vk::Format format) -> PipelineBuilder&;
        auto setColorAttachmentFormats(std::span<const vk::Format> formats) -> PipelineBuilder&;
        auto disableColorAttachments() -> PipelineBuilder&;
        auto setColorBlendAttachment(uint32_t index, vk::PipelineColorBlendAttachmentState attachment)
            -> PipelineBuilder&;
        auto enableAlphaBlending(uint32_t index = 0) -> PipelineBuilder&;
        auto disableBlending(uint32_t index = 0) -> PipelineBuilder&;
        auto enableDepth(
            vk::Format format,
            bool writeDepth = true,
            vk::CompareOp compareOp = vk::CompareOp::eLessOrEqual) -> PipelineBuilder&;
        auto disableDepth() -> PipelineBuilder&;
        auto setStencilFormat(vk::Format format) -> PipelineBuilder&;
        auto setDynamicStates(std::span<const vk::DynamicState> states) -> PipelineBuilder&;
        auto addDescriptorSetLayout(vk::DescriptorSetLayout layout) -> PipelineBuilder&;
        auto addDescriptorSetLayout(DescriptorSetLayout const& layout) -> PipelineBuilder&;
        auto setDescriptorSetLayouts(std::span<const vk::DescriptorSetLayout> layouts) -> PipelineBuilder&;
        auto addPushConstantRange(vk::PushConstantRange range) -> PipelineBuilder&;
        auto setPushConstantRanges(std::span<const vk::PushConstantRange> ranges) -> PipelineBuilder&;
        auto buildPipelineLayout(vk::raii::Device const& device) const -> vk::raii::PipelineLayout;
        auto buildGraphicsPipeline(vk::raii::Device const& device, vk::PipelineLayout layout) const
            -> vk::raii::Pipeline;
        auto buildGraphics(vk::raii::Device const& device) const -> PipelineBuildResult;
        auto buildComputePipeline(vk::raii::Device const& device, vk::PipelineLayout layout) const
            -> vk::raii::Pipeline;
        auto buildCompute(vk::raii::Device const& device) const -> PipelineBuildResult;
    };

}  // namespace Aerkanis::Vk
