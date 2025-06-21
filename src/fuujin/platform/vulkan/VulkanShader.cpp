#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanShader.h"

#include "fuujin/renderer/Renderer.h"

#include "fuujin/platform/vulkan/VulkanContext.h"

#include <spirv_cross.hpp>

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

        Reflect();
    }

    VulkanShader::~VulkanShader() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto modules = m_Modules;
        auto pipelineLayout = m_PipelineLayout;
        auto setLayouts = m_SetLayouts;

        Renderer::Submit([=]() {
            auto callbacks = &VulkanContext::GetAllocCallbacks();
            vkDestroyPipelineLayout(device, pipelineLayout, callbacks);

            for (auto [stage, module] : modules) {
                vkDestroyShaderModule(device, module, callbacks);
            }

            for (auto [index, layout] : setLayouts) {
                vkDestroyDescriptorSetLayout(device, layout, callbacks);
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

    void VulkanShader::Reflect() {
        ZoneScoped;

        m_PushConstants.TotalSize = 0;

        for (const auto& [stage, code] : m_Code) {
            spirv_cross::Compiler compiler((const uint32_t*)code.data(),
                                           code.size() / sizeof(uint32_t));

            auto resources = compiler.get_shader_resources();

            ProcessResources(resources.uniform_buffers, compiler, stage, UniformBuffer);
            ProcessResources(resources.storage_buffers, compiler, stage, StorageBuffer);
            ProcessResources(resources.separate_images, compiler, stage, SampledImage);
            ProcessResources(resources.separate_samplers, compiler, stage, ShaderSampler);
            ProcessResources(resources.storage_images, compiler, stage, StorageImage);
            ProcessResources(resources.sampled_images, compiler, stage,
                             SampledImage | ShaderSampler);

            ProcessPushConstants(resources.push_constant_buffers, compiler, stage);

            if (stage == ShaderStage::Vertex) {
                ProcessVertexInputs(resources.stage_inputs, compiler, stage);
            }
        }

        Renderer::Submit([this]() { RT_CreateLayouts(); }, "Create pipeline-bound layouts");
    }

    void VulkanShader::ProcessType(spirv_cross::TypeID id, const spirv_cross::Compiler& compiler,
                                   ShaderStage stage) {
        ZoneScoped;

        auto& types = m_Types[stage];
        if (types.contains(id)) {
            return;
        }

        const auto& type = compiler.get_type(id);
        auto& result = types[id];

        result.BaseType = type.basetype;
        result.Rows = type.vecsize;
        result.Columns = type.columns;

        result.ElementSize = 0;
        switch (result.BaseType) {
        case spirv_cross::SPIRType::Void:
            result.Name = "void";
            break;
        case spirv_cross::SPIRType::Boolean:
            result.Name = "bool";
            result.ElementSize = 1;
            break;
        case spirv_cross::SPIRType::SByte:
            result.Name = "sbyte";
            result.ElementSize = 1;
            break;
        case spirv_cross::SPIRType::UByte:
            result.Name = "byte";
            result.ElementSize = 1;
            break;
        case spirv_cross::SPIRType::Short:
            result.Name = "short";
            result.ElementSize = 2;
            break;
        case spirv_cross::SPIRType::UShort:
            result.Name = "ushort";
            result.ElementSize = 2;
            break;
        case spirv_cross::SPIRType::Int:
            result.Name = "int";
            result.ElementSize = 4;
            break;
        case spirv_cross::SPIRType::UInt:
            result.Name = "uint";
            result.ElementSize = 4;
            break;
        case spirv_cross::SPIRType::Int64:
            result.Name = "int64";
            result.ElementSize = 8;
            break;
        case spirv_cross::SPIRType::UInt64:
            result.Name = "uint64";
            result.ElementSize = 8;
            break;
        case spirv_cross::SPIRType::AtomicCounter:
            result.Name = "atomic";
            break;
        case spirv_cross::SPIRType::Half:
            result.Name = "half";
            result.ElementSize = 2;
            break;
        case spirv_cross::SPIRType::Float:
            result.Name = "float";
            result.ElementSize = 4;
            break;
        case spirv_cross::SPIRType::Double:
            result.Name = "double";
            result.ElementSize = 8;
            break;
        case spirv_cross::SPIRType::Struct:
            result.Name = compiler.get_name(id);
            result.ElementSize = compiler.get_declared_struct_size(type);

            for (size_t i = 0; i < type.member_types.size(); i++) {
                auto memberTypeID = type.member_types[i];
                const auto& memberType = compiler.get_type(memberTypeID);

                ShaderField field;
                field.Type = memberType.self;
                field.Offset = compiler.type_struct_member_offset(type, i);
                field.Stride = memberType.array.empty()
                                   ? 0
                                   : compiler.type_struct_member_array_stride(type, i);

                for (uint32_t dimension : memberType.array) {
                    field.Dimensions.push_back(dimension);
                }

                auto name = compiler.get_member_name(id, i);
                if (name.empty()) {
                    name = "<field_" + std::to_string(i) + ">";
                }

                result.Fields[name] = field;
                ProcessType(field.Type, compiler, stage);
            }
            break;
        case spirv_cross::SPIRType::Image:
            result.Name = "image";
            break;
        case spirv_cross::SPIRType::SampledImage:
            result.Name = "sampledimage";
            break;
        case spirv_cross::SPIRType::Sampler:
            result.Name = "sampler";
            break;
        }

        result.TotalSize = result.ElementSize * result.Rows * result.Columns;
    }

    void VulkanShader::ProcessResources(
        const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
        const spirv_cross::Compiler& compiler, ShaderStage stage, uint32_t type) {
        ZoneScoped;

        for (const auto& resource : resources) {
            uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

            auto& setData = m_Resources[set];
            bool exists = setData.contains(binding);

            auto& result = setData[binding];
            bool canOverwrite =
                (result.ResourceType & SampledImage) == 0 && (type & SampledImage) != 0;

            result.ResourceType |= type;
            if (!exists || canOverwrite) {
                result.Name = resource.name;
                if (result.Name.empty()) {
                    result.Name = compiler.get_name(resource.id);
                }

                const auto& type = compiler.get_type(resource.type_id);
                result.Dimensions.clear();
                for (uint32_t dimension : type.array) {
                    result.Dimensions.push_back(dimension);
                }
            }

            if (!result.Types.contains(stage) || canOverwrite) {
                result.Types[stage] = resource.base_type_id;
                ProcessType(resource.base_type_id, compiler, stage);
            }
        }
    }

    void VulkanShader::ProcessPushConstants(
        const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
        const spirv_cross::Compiler& compiler, ShaderStage stage) {
        ZoneScoped;

        if (resources.empty()) {
            return;
        }

        spirv_cross::TypeID id = resources[0].base_type_id;
        ProcessType(id, compiler, stage);

        m_PushConstants.Types[stage] = id;

        const auto& type = m_Types.at(stage).at(id);
        if (type.TotalSize > m_PushConstants.TotalSize) {
            m_PushConstants.TotalSize = type.TotalSize;
        }
    }

    struct AttributeData {
        size_t Size;
        VkFormat Format;
    };

    void VulkanShader::ProcessVertexInputs(
        const spirv_cross::SmallVector<spirv_cross::Resource>& inputs,
        const spirv_cross::Compiler& compiler, ShaderStage stage) {
        ZoneScoped;

        std::map<uint32_t, AttributeData> attributes;
        for (const auto& resource : inputs) {
            uint32_t location = compiler.get_decoration(resource.id, spv::DecorationLocation);
            auto& attr = attributes[location];

            ProcessType(resource.base_type_id, compiler, stage);
            const auto& type = m_Types.at(stage).at(resource.base_type_id);

            attr.Size = type.TotalSize;

            bool found = true;
            switch (type.BaseType) {
            case spirv_cross::SPIRType::Float:
                switch (type.Rows) {
                case 1:
                    attr.Format = VK_FORMAT_R32_SFLOAT;
                    break;
                case 2:
                    attr.Format = VK_FORMAT_R32G32_SFLOAT;
                    break;
                case 3:
                    attr.Format = VK_FORMAT_R32G32B32_SFLOAT;
                    break;
                case 4:
                    attr.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
                    break;
                default:
                    found = false;
                    break;
                }
                break;
            case spirv_cross::SPIRType::Int:
                switch (type.Rows) {
                case 1:
                    attr.Format = VK_FORMAT_R32_SINT;
                    break;
                case 2:
                    attr.Format = VK_FORMAT_R32G32_SINT;
                    break;
                case 3:
                    attr.Format = VK_FORMAT_R32G32B32_SINT;
                    break;
                case 4:
                    attr.Format = VK_FORMAT_R32G32B32A32_SINT;
                    break;
                default:
                    found = false;
                    break;
                }
                break;
            case spirv_cross::SPIRType::UInt:
                switch (type.Rows) {
                case 1:
                    attr.Format = VK_FORMAT_R32_SINT;
                    break;
                case 2:
                    attr.Format = VK_FORMAT_R32G32_SINT;
                    break;
                case 3:
                    attr.Format = VK_FORMAT_R32G32B32_SINT;
                    break;
                case 4:
                    attr.Format = VK_FORMAT_R32G32B32A32_SINT;
                    break;
                default:
                    found = false;
                    break;
                }
                break;
            default:
                found = false;
                break;
            }

            if (!found) {
                attr.Format = VK_FORMAT_UNDEFINED;
            }
        }

        size_t offset = 0;
        m_VertexAttributes.clear();

        for (const auto& [location, attr] : attributes) {
            VkVertexInputAttributeDescription desc{};
            desc.location = location;
            desc.format = attr.Format;
            desc.binding = 0;
            desc.offset = (uint32_t)offset;

            m_VertexAttributes.push_back(desc);
            offset += attr.Size;
        }

        std::memset(&m_VertexBinding, 0, sizeof(VkVertexInputBindingDescription));
        m_VertexBinding.binding = 0;
        m_VertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        m_VertexBinding.stride = (uint32_t)offset;

        std::memset(&m_VertexInput, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
        m_VertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        m_VertexInput.vertexBindingDescriptionCount = 1;
        m_VertexInput.pVertexBindingDescriptions = &m_VertexBinding;
        m_VertexInput.vertexAttributeDescriptionCount = (uint32_t)m_VertexAttributes.size();
        m_VertexInput.pVertexAttributeDescriptions = m_VertexAttributes.data();
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

    void VulkanShader::RT_CreateLayouts() {
        ZoneScoped;

        auto callbacks = &VulkanContext::GetAllocCallbacks();
        for (const auto& [setIndex, resources] : m_Resources) {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for (const auto& [bindingIndex, resource] : resources) {
                size_t descriptorCount = 1;
                for (size_t dimension : resource.Dimensions) {
                    descriptorCount *= dimension;
                }

                VkDescriptorSetLayoutBinding binding{};
                binding.binding = bindingIndex;
                binding.descriptorCount = (uint32_t)descriptorCount;

                for (auto [stage, type] : resource.Types) {
                    binding.stageFlags |= ConvertStage(stage);
                }

                switch (resource.ResourceType) {
                case UniformBuffer:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    break;
                case StorageBuffer:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    break;
                case SampledImage:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    break;
                case ShaderSampler:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    break;
                case SampledImage | ShaderSampler:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    break;
                case StorageImage:
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    break;
                default:
                    throw std::runtime_error("Unknown descriptor type!");
                }

                bindings.push_back(binding);
            }

            VkDescriptorSetLayoutCreateInfo setInfo{};
            setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            setInfo.bindingCount = (uint32_t)bindings.size();
            setInfo.pBindings = bindings.data();

            VkDescriptorSetLayout setLayout;
            if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &setInfo, callbacks,
                                            &setLayout) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create descriptor set layout!");
            }

            m_SetLayouts.insert(std::make_pair(setIndex, setLayout));
        }

        std::vector<VkDescriptorSetLayout> layouts;
        for (auto [setIndex, setLayout] : m_SetLayouts) {
            size_t index = layouts.size();
            if (index != setIndex) {
                throw std::runtime_error("Vector and map index mismatch! " + std::to_string(index) +
                                         " != " + std::to_string(setIndex));
            }

            layouts.push_back(setLayout);
        }

        VkPushConstantRange range{};
        range.size = (uint32_t)m_PushConstants.TotalSize;

        for (auto [stage, type] : m_PushConstants.Types) {
            range.stageFlags |= ConvertStage(stage);
        }

        VkPipelineLayoutCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineInfo.setLayoutCount = (uint32_t)layouts.size();
        pipelineInfo.pSetLayouts = layouts.data();

        if (range.size > 0) {
            pipelineInfo.pushConstantRangeCount = 1;
            pipelineInfo.pPushConstantRanges = &range;
        }

        if (vkCreatePipelineLayout(m_Device->GetDevice(), &pipelineInfo, callbacks,
                                   &m_PipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }
    }
} // namespace fuujin