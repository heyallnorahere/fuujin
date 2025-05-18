#pragma once
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/renderer/Shader.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    class VulkanShader : public Shader {
    public:
        static VkShaderStageFlagBits ConvertStage(ShaderStage stage);

        VulkanShader(Ref<VulkanDevice> device, const Code& code);

        virtual ~VulkanShader() override;

        virtual void GetStages(std::unordered_set<ShaderStage>& stages) const override;

        const std::unordered_map<ShaderStage, VkShaderModule>& GetModules() const {
            return m_Modules;
        }

    private:
        void RT_Load();

        Ref<VulkanDevice> m_Device;
        Code m_Code;

        std::unordered_map<ShaderStage, VkShaderModule> m_Modules;
    };
} // namespace fuujin