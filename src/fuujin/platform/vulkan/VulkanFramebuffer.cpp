#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanFramebuffer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    VulkanFramebuffer::VulkanFramebuffer(Ref<VulkanDevice> device, const VulkanSpec& spec) {
        ZoneScoped;

        m_Device = device;
        m_Spec = spec;

        Renderer::Submit([&]() { RT_Create(); });
    }

    VulkanFramebuffer::~VulkanFramebuffer() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto framebuffer = m_Framebuffer;

        Renderer::Submit([device, framebuffer]() {
            vkDestroyFramebuffer(device, framebuffer, &VulkanContext::GetAllocCallbacks());
        });
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
} // namespace fuujin