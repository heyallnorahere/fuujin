#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanPipeline.h"

#include "fuujin/renderer/Renderer.h"

#include "fuujin/platform/vulkan/VulkanContext.h"
#include "fuujin/platform/vulkan/VulkanShader.h"
#include "fuujin/platform/vulkan/VulkanFramebuffer.h"
#include "fuujin/platform/vulkan/VulkanSwapchain.h"

namespace fuujin {
    VulkanPipeline::VulkanPipeline(Ref<VulkanDevice> device, const Spec& spec) {
        ZoneScoped;

        m_Device = device;
        m_Spec = spec;

        m_Pipeline = VK_NULL_HANDLE;

        Renderer::Submit([this]() { RT_Create(); });
    }

    VulkanPipeline::~VulkanPipeline() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto pipeline = m_Pipeline;

        Renderer::Submit(
            [=]() { vkDestroyPipeline(device, pipeline, &VulkanContext::GetAllocCallbacks()); });
    }

    void VulkanPipeline::RT_Create() {
        ZoneScoped;

        switch (m_Spec.PipelineType) {
        case Type::Compute:
            RT_CreateComputePipeline();
            break;
        case Type::Graphics:
            RT_CreateGraphicsPipeline();
            break;
        default:
            throw std::runtime_error("Invalid pipeline type!");
        }
    }

    void VulkanPipeline::RT_CreateComputePipeline() {
        ZoneScoped;

        auto shader = m_Spec.PipelineShader.As<VulkanShader>();
        const auto& modules = shader->GetModules();

        if (!modules.contains(ShaderStage::Compute)) {
            throw std::runtime_error("Passed shader has no compute stage!");
        }

        VkComputePipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        createInfo.layout = shader->GetPipelineLayout();
        createInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        createInfo.stage.module = modules.at(ShaderStage::Compute);
        createInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

        if (vkCreateComputePipelines(m_Device->GetDevice(), VK_NULL_HANDLE, 1, &createInfo,
                                     &VulkanContext::GetAllocCallbacks(),
                                     &m_Pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline!");
        }
    }

    void VulkanPipeline::RT_CreateGraphicsPipeline() {
        ZoneScoped;

        Ref<VulkanRenderPass> renderPass;
        switch (m_Spec.Target->GetType()) {
        case RenderTargetType::Framebuffer:
            renderPass = m_Spec.Target.As<VulkanFramebuffer>()->GetRenderPass();
            break;
        case RenderTargetType::Swapchain:
            renderPass = m_Spec.Target.As<VulkanSwapchain>()->GetRenderPass();
            break;
        default:
            throw std::runtime_error("Invalid render target type!");
        }

        auto shader = m_Spec.PipelineShader.As<VulkanShader>();
        const auto& modules = shader->GetModules();

        std::vector<VkPipelineShaderStageCreateInfo> shaderCreateInfo;
        for (const auto& [stage, module] : modules) {
            VkPipelineShaderStageCreateInfo moduleInfo{};
            moduleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            moduleInfo.stage = VulkanShader::ConvertStage(stage);
            moduleInfo.pName = "main";
            moduleInfo.module = module;

            shaderCreateInfo.push_back(moduleInfo);
        }

        static const std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR,
                                                                   VK_DYNAMIC_STATE_VIEWPORT };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

        std::vector<VkVertexInputAttributeDescription> vertexAttributes;
        std::vector<VkVertexInputBindingDescription> vertexBindings;
        std::map<uint32_t, size_t> bindingOffsets;

        const auto& attributes = shader->GetVertexAttributes();
        for (const auto& [location, attr] : attributes) {
            uint32_t binding = 0;
            if (m_Spec.AttributeBindings.contains(location)) {
                binding = m_Spec.AttributeBindings.at(location);
            }

            VkVertexInputAttributeDescription desc{};
            desc.location = location;
            desc.format = attr.Format;
            desc.binding = binding;

            if (bindingOffsets.contains(binding)) {
                desc.offset = (uint32_t)bindingOffsets.at(binding);
            } else {
                bindingOffsets[binding] = 0;
            }

            vertexAttributes.push_back(desc);
            bindingOffsets[binding] += attr.Size;
        }

        for (const auto& [binding, stride] : bindingOffsets) {
            VkVertexInputBindingDescription desc{};
            desc.binding = binding;
            desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            desc.stride = (uint32_t)stride;

            vertexBindings.push_back(desc);
        }

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = (uint32_t)vertexBindings.size();
        vertexInput.pVertexBindingDescriptions = vertexBindings.data();
        vertexInput.vertexAttributeDescriptionCount = (uint32_t)vertexAttributes.size();
        vertexInput.pVertexAttributeDescriptions = vertexAttributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.cullMode = m_Spec.DisableCulling ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        rasterization.polygonMode = m_Spec.Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        rasterization.lineWidth = 1.f;

        switch (m_Spec.PolygonFrontFace) {
        case FrontFace::CCW:
            rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        case FrontFace::CW:
            rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
            break;
        default:
            throw std::runtime_error("Invalid front face!");
        }

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = (VkSampleCountFlagBits)m_Spec.Target->GetSamples();

        VkPhysicalDeviceFeatures2 features{};
        m_Device->RT_GetFeatures(features);

        if (features.features.sampleRateShading) {
            multisampling.sampleShadingEnable = VK_TRUE;
            multisampling.minSampleShading = 0.02f;
        }

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        if (m_Spec.Depth.Test) {
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        }

        if (m_Spec.Depth.Write) {
            depthStencil.depthWriteEnable = VK_TRUE;
        }

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.logicOpEnable = VK_FALSE;

        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &colorBlendAttachment;

        if (m_Spec.Blending != ColorBlending::None) {
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            switch (m_Spec.Blending) {
            case ColorBlending::Default:
                colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                break;
            case ColorBlending::Additive:
                colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                break;
            case ColorBlending::OneZero:
                colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                break;
            case ColorBlending::ZeroSourceColor:
                colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
                break;
            }

            colorBlendAttachment.srcAlphaBlendFactor = colorBlendAttachment.srcColorBlendFactor;
            colorBlendAttachment.dstAlphaBlendFactor = colorBlendAttachment.dstColorBlendFactor;
        }

        VkGraphicsPipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.layout = shader->GetPipelineLayout();
        createInfo.renderPass = renderPass->Get();
        createInfo.stageCount = (uint32_t)shaderCreateInfo.size();
        createInfo.pStages = shaderCreateInfo.data();
        createInfo.pVertexInputState = &vertexInput;
        createInfo.pInputAssemblyState = &inputAssembly;
        createInfo.pTessellationState = nullptr;
        createInfo.pViewportState = &viewport;
        createInfo.pRasterizationState = &rasterization;
        createInfo.pMultisampleState = &multisampling;
        createInfo.pDepthStencilState = &depthStencil;
        createInfo.pColorBlendState = &colorBlend;
        createInfo.pDynamicState = &dynamicState;

        if (vkCreateGraphicsPipelines(m_Device->GetDevice(), VK_NULL_HANDLE, 1, &createInfo,
                                      &VulkanContext::GetAllocCallbacks(),
                                      &m_Pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
    }
} // namespace fuujin
