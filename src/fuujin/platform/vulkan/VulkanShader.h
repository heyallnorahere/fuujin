#pragma once
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/renderer/Shader.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

#include <spirv_cross.hpp>

namespace fuujin {
    enum ShaderResourceType : uint32_t {
        UniformBuffer = 1 << 0,
        StorageBuffer = 1 << 1,
        SampledImage = 1 << 2,
        ShaderSampler = 1 << 3,
        StorageImage = 1 << 4,
    };

    struct ShaderResource {
        uint32_t ResourceType;
        std::string Name;
        std::unordered_map<ShaderStage, spirv_cross::TypeID> Types;
        std::vector<size_t> Dimensions;
    };

    struct ShaderPushConstants {
        std::unordered_map<ShaderStage, spirv_cross::TypeID> Types;
        size_t TotalSize;
    };

    struct ShaderField {
        spirv_cross::TypeID Type;
        size_t Offset, Stride;
        std::vector<size_t> Dimensions;
    };

    struct ShaderType {
        std::string Name;
        size_t ElementSize;
        size_t Rows, Columns;
        size_t TotalSize;
        std::unordered_map<std::string, ShaderField> Fields;
        spirv_cross::SPIRType::BaseType BaseType;
    };

    class VulkanShader : public Shader {
    public:
        static VkShaderStageFlagBits ConvertStage(ShaderStage stage);

        VulkanShader(Ref<VulkanDevice> device, const Code& code);

        virtual ~VulkanShader() override;

        virtual void GetStages(std::unordered_set<ShaderStage>& stages) const override;

        const std::unordered_map<ShaderStage, VkShaderModule>& GetModules() const {
            return m_Modules;
        }

        VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

        const std::map<uint32_t, VkDescriptorSetLayout>& GetDescriptorSetLayouts() const {
            return m_SetLayouts;
        }

        const VkPipelineVertexInputStateCreateInfo& GetVertexInput() const { return m_VertexInput; }

    private:
        void Reflect();
        void ProcessType(spirv_cross::TypeID id, const spirv_cross::Compiler& compiler,
                         ShaderStage stage);

        void ProcessResources(const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                              const spirv_cross::Compiler& compiler, ShaderStage stage,
                              uint32_t type);

        void ProcessPushConstants(const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                                  const spirv_cross::Compiler& compiler, ShaderStage stage);

        void ProcessVertexInputs(const spirv_cross::SmallVector<spirv_cross::Resource>& inputs,
                                 const spirv_cross::Compiler& compiler, ShaderStage stage);

        void RT_Load();
        void RT_CreateLayouts();

        Ref<VulkanDevice> m_Device;
        Code m_Code;

        std::unordered_map<ShaderStage, VkShaderModule> m_Modules;
        std::map<uint32_t, VkDescriptorSetLayout> m_SetLayouts;
        VkPipelineLayout m_PipelineLayout;

        VkPipelineVertexInputStateCreateInfo m_VertexInput;
        VkVertexInputBindingDescription m_VertexBinding;
        std::vector<VkVertexInputAttributeDescription> m_VertexAttributes;

        std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource>> m_Resources;
        ShaderPushConstants m_PushConstants;
        std::unordered_map<ShaderStage, std::unordered_map<spirv_cross::TypeID, ShaderType>>
            m_Types;
    };
} // namespace fuujin