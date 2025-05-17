#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanFramebuffer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    VulkanFramebuffer::VulkanFramebuffer(Ref<VulkanDevice> device, const VulkanSpec& spec) {
        ZoneScoped;

        m_Device = device;
        m_Spec = spec;

        Renderer::Submit([&]() { RT_Create(); }, "Create framebuffer");
    }

    VulkanFramebuffer::~VulkanFramebuffer() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto framebuffer = m_Framebuffer;

        Renderer::Submit(
            [device, framebuffer]() {
                vkDestroyFramebuffer(device, framebuffer, &VulkanContext::GetAllocCallbacks());
            },
            "Destroy framebuffer");
    }

    void VulkanFramebuffer::AddImageDependency(CommandList& cmdList) {
        ZoneScoped;

        auto semaphore = GetSemaphore();
        if (semaphore.IsEmpty()) {
            return;
        }

        auto buffer = (VulkanCommandBuffer*)&cmdList;
        buffer->AddWaitSemaphore(semaphore, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                VK_PIPELINE_STAGE_TRANSFER_BIT |
                                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }

    void VulkanFramebuffer::RT_BeginRender(CommandList& cmdList) {
        ZoneScoped;

        // uhh
        auto buffer = (VulkanCommandBuffer*)&cmdList;
        RT_BeginRenderPass(buffer, glm::vec4(0.f));
    }

    void VulkanFramebuffer::RT_EndRender(CommandList& cmdList) {
        ZoneScoped;

        auto buffer = (VulkanCommandBuffer*)&cmdList;
        RT_EndRenderPass(buffer);

        auto semaphore = GetSemaphore();
        if (semaphore.IsPresent()) {
            buffer->AddSemaphore(semaphore, SemaphoreUsage::Signal);
        }
    }

    void VulkanFramebuffer::RT_BeginRenderPass(VulkanCommandBuffer* buffer,
                                               const glm::vec4& clear) const {
        ZoneScoped;

        std::vector<VkClearValue> clearValues;
        clearValues.resize(2, {});

        std::memcpy(clearValues[0].color.float32, &clear.x, sizeof(float) * 4);
        clearValues[1].depthStencil = { 1.f, 0 };

        VkRenderPassBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.clearValueCount = (uint32_t)clearValues.size();
        beginInfo.pClearValues = clearValues.data();
        beginInfo.framebuffer = m_Framebuffer;
        beginInfo.renderPass = m_Spec.RenderPass->Get();
        beginInfo.renderArea.extent = m_Spec.Extent;

        vkCmdBeginRenderPass(buffer->Get(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanFramebuffer::RT_EndRenderPass(VulkanCommandBuffer* buffer) const {
        ZoneScoped;

        vkCmdEndRenderPass(buffer->Get());
    }

    void VulkanFramebuffer::RT_Create() {
        ZoneScoped;

        std::vector<VkImageView> views;
        for (auto image : m_Spec.Attachments) {
            views.push_back(image->GetView());
        }

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.flags = m_Spec.Flags;
        createInfo.renderPass = m_Spec.RenderPass->Get();
        createInfo.attachmentCount = (uint32_t)views.size();
        createInfo.pAttachments = views.data();
        createInfo.width = m_Spec.Extent.width;
        createInfo.height = m_Spec.Extent.height;
        createInfo.layers = m_Spec.Layers;

        if (vkCreateFramebuffer(m_Device->GetDevice(), &createInfo,
                                &VulkanContext::GetAllocCallbacks(),
                                &m_Framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }

    Ref<VulkanSemaphore> VulkanFramebuffer::GetSemaphore() const {
        ZoneScoped;

        Ref<VulkanSemaphore> semaphore;
        if (m_Spec.SemaphoreCallback.has_value()) {
            semaphore = m_Spec.SemaphoreCallback.value()();
        }

        return semaphore;
    }
} // namespace fuujin