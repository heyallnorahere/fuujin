#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanRenderer.h"

#include "fuujin/platform/vulkan/VulkanBuffer.h"
#include "fuujin/platform/vulkan/VulkanPipeline.h"
#include "fuujin/platform/vulkan/VulkanCommandQueue.h"

namespace fuujin {
    VulkanRenderer::VulkanRenderer(Ref<VulkanDevice> device) {
        ZoneScoped;

        m_Device = device;

        // todo: descriptor pools and such
    }

    VulkanRenderer::~VulkanRenderer() {
        ZoneScoped;

        // todo: clean up descriptor pools
    }

    void VulkanRenderer::RT_RenderIndexed(CommandList& cmdlist, Ref<DeviceBuffer> vertices,
                                          Ref<DeviceBuffer> indices, Ref<Pipeline> pipeline,
                                          uint32_t indexCount, const Buffer& pushConstants) const {
        ZoneScoped;

        cmdlist.AddDependency(vertices);
        cmdlist.AddDependency(indices);
        cmdlist.AddDependency(pipeline);

        // todo: tracy gpu zone

        auto vertexBuffer = vertices.As<VulkanBuffer>()->Get();
        auto indexBuffer = indices.As<VulkanBuffer>()->Get();
        auto vkPipeline = pipeline.As<VulkanPipeline>();
        auto shader = vkPipeline->GetSpec().Shader.As<VulkanShader>();
        auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->GetPipeline());

        // no descriptor sets

        if (pushConstants) {
            const auto& shaderData = shader->GetPushConstants();

            VkShaderStageFlags stages = 0;
            for (auto [stage, id] : shaderData.Types) {
                stages |= VulkanShader::ConvertStage(stage);
            }

            if (stages != 0) {
                vkCmdPushConstants(cmdBuffer, shader->GetPipelineLayout(), stages, 0,
                                   (uint32_t)pushConstants.GetSize(), pushConstants.Get());
            }
        }

        vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);
    }

    void VulkanRenderer::RT_SetViewport(CommandList& cmdlist, Ref<RenderTarget> target) const {
        ZoneScoped;

        uint32_t width = target->GetWidth();
        uint32_t height = target->GetHeight();

        VkRect2D scissor{};
        scissor.extent.width = width;
        scissor.extent.height = height;

        VkViewport viewport{};
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        switch (target->GetType()) {
        case RenderTargetType::Swapchain:
            viewport.x = 0.f;
            viewport.y = (float)height;
            viewport.width = (float)width;
            viewport.height = -(float)height;
            break;
        case RenderTargetType::Framebuffer:
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float)width;
            viewport.height = (float)height;
            break;
        default:
            throw std::runtime_error("Invalid render target type!");
        }

        auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    }
} // namespace fuujin