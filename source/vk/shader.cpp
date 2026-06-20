#include "vk/shader.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace Aerkanis::Vk
{

    auto readSpirvFile(std::filesystem::path const& path) -> std::vector<uint32_t>
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("failed to open SPIR-V file: " + path.string());
        }

        const std::ifstream::pos_type fileSize = file.tellg();
        if (fileSize <= 0)
        {
            throw std::runtime_error("SPIR-V file is empty: " + path.string());
        }

        if ((static_cast<std::size_t>(fileSize) % sizeof(uint32_t)) != 0)
        {
            throw std::runtime_error("SPIR-V file size is not aligned to uint32_t: " + path.string());
        }

        std::vector<uint32_t> code(static_cast<std::size_t>(fileSize) / sizeof(uint32_t));
        file.seekg(0);
        file.read(
            reinterpret_cast<char*>(code.data()),
            static_cast<std::streamsize>(static_cast<std::size_t>(fileSize)));

        if (!file)
        {
            throw std::runtime_error("failed to read SPIR-V file: " + path.string());
        }

        return code;
    }

    auto ShaderModule::init(vk::raii::Device const& device, std::filesystem::path const& path) -> void
    {
        const std::vector<uint32_t> code = readSpirvFile(path);
        init(device, code);
    }

    auto ShaderModule::init(vk::raii::Device const& device, std::span<const uint32_t> code) -> void
    {
        shutdown();

        if (code.empty())
        {
            throw std::runtime_error("ShaderModule requires non-empty SPIR-V code");
        }

        vk::ShaderModuleCreateInfo shaderModuleCreateInfo{
            .codeSize = code.size_bytes(),
            .pCode = code.data(),
        };

        shaderModule = vk::raii::ShaderModule(device, shaderModuleCreateInfo);
    }

    auto ShaderModule::shutdown() noexcept -> void
    {
        shaderModule.clear();
    }

    auto ShaderModule::stage(
        vk::ShaderStageFlagBits shaderStage,
        const char* entryPoint,
        vk::SpecializationInfo const* specializationInfo) const
        -> vk::PipelineShaderStageCreateInfo
    {
        if (entryPoint == nullptr)
        {
            throw std::runtime_error("Shader stage requires a valid entry point");
        }

        return vk::PipelineShaderStageCreateInfo{
            .stage = shaderStage,
            .module = handle(),
            .pName = entryPoint,
            .pSpecializationInfo = specializationInfo,
        };
    }

    auto ShaderModule::handle() const noexcept -> vk::ShaderModule
    {
        return static_cast<vk::ShaderModule>(*shaderModule);
    }

}  // namespace Aerkanis::Vk
