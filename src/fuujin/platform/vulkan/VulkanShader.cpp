#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanShader.h"

#include "fuujin/renderer/Renderer.h"

#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    VkShaderStageFlagBits VulkanShader::ConvertStage(ShaderStage stage) {
        ZoneScoped;

        switch (stage) {
        case ShaderStage::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        default:
            throw std::runtime_error("Invalid shader stage!");
        }
    }

    VulkanShader::VulkanShader(Ref<VulkanDevice> device, const Code& code) {
        ZoneScoped;

        m_Device = device;
        m_Code = code; // we need a copy

        Renderer::Submit([=]() { RT_Load(); }, "Load shader modules");
    }

    VulkanShader::~VulkanShader() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto modules = m_Modules;

        Renderer::Submit([=]() {
            for (const auto& [stage, module] : modules) {
                vkDestroyShaderModule(device, module, &VulkanContext::GetAllocCallbacks());
            }
        });
    }

    void VulkanShader::GetStages(std::unordered_set<ShaderStage>& stages) const {
        ZoneScoped;

        stages.clear();
        for (const auto& [stage, module] : m_Modules) {
            stages.insert(stage);
        }
    }

    void VulkanShader::RT_Load() {
        ZoneScoped;

        for (const auto& [stage, code] : m_Code) {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.pCode = (const uint32_t*)code.data();
            createInfo.codeSize = code.size();

            VkShaderModule module;
            if (vkCreateShaderModule(m_Device->GetDevice(), &createInfo,
                                     &VulkanContext::GetAllocCallbacks(), &module) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create shader module!");
            }

            m_Modules.insert(std::make_pair(stage, module));
        }
    }
} // namespace fuujin