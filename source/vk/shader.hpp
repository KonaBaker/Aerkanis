#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Aerkanis::Vk
{

    auto readSpirvFile(std::filesystem::path const& path) -> std::vector<uint32_t>;

    struct ShaderModule
    {
        vk::raii::ShaderModule shaderModule{nullptr};

        auto init(vk::raii::Device const& device, std::filesystem::path const& path) -> void;
        auto init(vk::raii::Device const& device, std::span<const uint32_t> code) -> void;
        auto shutdown() noexcept -> void;
        auto stage(
            vk::ShaderStageFlagBits shaderStage,
            const char* entryPoint = "main",
            vk::SpecializationInfo const* specializationInfo = nullptr) const
            -> vk::PipelineShaderStageCreateInfo;
        auto handle() const noexcept -> vk::ShaderModule;
    };

}  // namespace Aerkanis::Vk
