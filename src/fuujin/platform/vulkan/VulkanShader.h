#pragma once
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/renderer/Shader.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

#include <spirv_cross.hpp>

namespace fuujin {
    struct ResourceDescriptor {
        ResourceDescriptor() = default;

        ResourceDescriptor(uint32_t set, uint32_t binding) {
            Set = set;
            Binding = binding;
        }

        uint32_t Set, Binding;
    };

    struct ShaderResource {
        uint32_t ResourceType;
        std::string Name;
        std::unordered_map<ShaderStage, spirv_cross::TypeID> Types;
        std::vector<size_t> Dimensions;
        std::shared_ptr<GPUResource> Interface;
    };

    struct ShaderPushConstants {
        std::unordered_map<ShaderStage, spirv_cross::TypeID> Types;
        size_t TotalSize;
        std::shared_ptr<GPUResource> Interface;
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
        std::shared_ptr<GPUType> Interface;
    };

    class VulkanShader : public Shader {
    public:
        using Resources =
            std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource>>;

        using Types =
            std::unordered_map<ShaderStage, std::unordered_map<spirv_cross::TypeID, ShaderType>>;

        static VkShaderStageFlagBits ConvertStage(ShaderStage stage);
        static VkDescriptorType ConvertDescriptorType(uint32_t resourceType);

        VulkanShader(Ref<VulkanDevice> device, const Code& code);

        virtual ~VulkanShader() override;

        virtual uint64_t GetID() const override { return m_ID; }

        virtual void GetStages(std::unordered_set<ShaderStage>& stages) const override;

        virtual std::shared_ptr<GPUResource> GetPushConstants() const override;

        virtual std::shared_ptr<GPUResource> GetResourceByName(
            const std::string& name) const override;
        
        std::shared_ptr<GPUResource> GetResourceByDescriptor(
            const ResourceDescriptor& descriptor) const;

        bool GetResourceDescriptor(const std::string& name, ResourceDescriptor& descriptor) const;

        const std::unordered_map<ShaderStage, VkShaderModule>& GetModules() const {
            return m_Modules;
        }

        VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

        const std::map<uint32_t, VkDescriptorSetLayout>& GetDescriptorSetLayouts() const {
            return m_SetLayouts;
        }

        const VkPipelineVertexInputStateCreateInfo& GetVertexInput() const { return m_VertexInput; }

        const Resources& GetResources() const { return m_Resources; }
        const Types& GetTypes() const { return m_Types; }
        const ShaderPushConstants& GetPushConstantData() const { return m_PushConstants; }

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
        uint64_t m_ID;

        std::unordered_map<ShaderStage, VkShaderModule> m_Modules;
        std::map<uint32_t, VkDescriptorSetLayout> m_SetLayouts;
        VkPipelineLayout m_PipelineLayout;

        VkPipelineVertexInputStateCreateInfo m_VertexInput;
        VkVertexInputBindingDescription m_VertexBinding;
        std::vector<VkVertexInputAttributeDescription> m_VertexAttributes;

        Resources m_Resources;
        Types m_Types;
        ShaderPushConstants m_PushConstants;
        std::unordered_map<std::string, ResourceDescriptor> m_ResourceMap;
    };
} // namespace fuujin