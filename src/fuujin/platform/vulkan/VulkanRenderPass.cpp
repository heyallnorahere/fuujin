#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanRenderPass.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    static uint32_t s_ID = 0;

    static void CopyRenderPassInfo(const VkRenderPassCreateInfo* src, VkRenderPassCreateInfo* dst) {
        ZoneScoped;
        if (src->pNext != nullptr) {
            throw std::runtime_error("pNext not supported");
        }

        std::memset(dst, 0, sizeof(VkRenderPassCreateInfo));
        dst->sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        dst->flags = src->flags;
        dst->attachmentCount = src->attachmentCount;
        dst->dependencyCount = src->dependencyCount;
        dst->subpassCount = src->subpassCount;

        size_t size = sizeof(VkAttachmentDescription) * dst->attachmentCount;
        dst->pAttachments = (VkAttachmentDescription*)allocate(size);
        std::memcpy((void*)dst->pAttachments, src->pAttachments, size);

        size = sizeof(VkSubpassDependency) * dst->dependencyCount;
        dst->pDependencies = (VkSubpassDependency*)allocate(size);
        std::memcpy((void*)dst->pDependencies, src->pDependencies, size);

        size = sizeof(VkSubpassDescription) * dst->subpassCount;
        auto subpasses = (VkSubpassDescription*)allocate(size);
        dst->pSubpasses = subpasses;

        for (uint32_t i = 0; i < dst->subpassCount; i++) {
            auto srcSubpass = &src->pSubpasses[i];
            auto dstSubpass = &subpasses[i];

            std::memset(dstSubpass, 0, sizeof(VkSubpassDescription));

#define COPY_ATTACHMENTS(C, R, T)                                                                  \
    dstSubpass->C = srcSubpass->C;                                                                 \
    if (dstSubpass->C != 0 && srcSubpass->R != nullptr) {                                          \
        dstSubpass->R = (T*)allocate(dstSubpass->C * sizeof(T));                                   \
        std::memcpy((void*)dstSubpass->R, srcSubpass->R, dstSubpass->C * sizeof(T));               \
    }

            COPY_ATTACHMENTS(inputAttachmentCount, pInputAttachments, VkAttachmentReference);
            COPY_ATTACHMENTS(colorAttachmentCount, pColorAttachments, VkAttachmentReference);
            COPY_ATTACHMENTS(colorAttachmentCount, pResolveAttachments, VkAttachmentReference);
            COPY_ATTACHMENTS(preserveAttachmentCount, pPreserveAttachments, uint32_t);

            if (srcSubpass->pDepthStencilAttachment != nullptr) {
                dstSubpass->pDepthStencilAttachment =
                    (VkAttachmentReference*)allocate(sizeof(VkAttachmentReference));
                std::memcpy((void*)dstSubpass->pDepthStencilAttachment,
                            srcSubpass->pDepthStencilAttachment, sizeof(VkAttachmentReference));
            }
        }
    }

    VulkanRenderPass::VulkanRenderPass(Ref<VulkanDevice> device, const VkRenderPassCreateInfo* createInfo) {
        ZoneScoped;

        m_Device = device;
        m_ID = s_ID++;
        m_RenderPass = VK_NULL_HANDLE;

        m_CreateInfo = (VkRenderPassCreateInfo*)allocate(sizeof(VkRenderPassCreateInfo));
        CopyRenderPassInfo(createInfo, m_CreateInfo);
        Renderer::Submit([&]() { RT_Create(); });
    }

    VulkanRenderPass::~VulkanRenderPass() {
        ZoneScoped;

        auto createInfo = m_CreateInfo;
        auto device = m_Device->GetDevice();
        auto renderPass = m_RenderPass;

        Renderer::Submit([=]() {
            vkDestroyRenderPass(device, renderPass, &VulkanContext::GetAllocCallbacks());

            for (uint32_t i = 0; i < m_CreateInfo->subpassCount; i++) {
                auto subpass = &m_CreateInfo->pSubpasses[i];
                freemem((void*)subpass->pInputAttachments);
                freemem((void*)subpass->pColorAttachments);
                freemem((void*)subpass->pResolveAttachments);
                freemem((void*)subpass->pDepthStencilAttachment);
                freemem((void*)subpass->pPreserveAttachments);
            }

            freemem((void*)m_CreateInfo->pAttachments);
            freemem((void*)m_CreateInfo->pDependencies);
            freemem((void*)m_CreateInfo->pSubpasses);
            freemem(m_CreateInfo);
        });
    }

    void VulkanRenderPass::RT_Create() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        if (vkCreateRenderPass(device, m_CreateInfo, &VulkanContext::GetAllocCallbacks(), &m_RenderPass)) {
            FUUJIN_CRITICAL("Failed to create render pass {}!", m_ID);
            return;
        }
    }
} // namespace fuujin