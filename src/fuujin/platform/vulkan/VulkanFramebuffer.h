#pragma once
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanImage.h"
#include "fuujin/platform/vulkan/VulkanRenderPass.h"

namespace fuujin {
    class VulkanFramebuffer : public RefCounted {
    public:
        struct VulkanSpec {
            Ref<VulkanRenderPass> RenderPass;
            std::vector<Ref<VulkanImage>> Attachments;
            VkExtent2D Extent;

            uint32_t Layers = 1;
            VkFramebufferCreateFlags Flags = 0;
        };

        VulkanFramebuffer(Ref<VulkanDevice> device, const VulkanSpec& spec);
        virtual ~VulkanFramebuffer() override;

        VkFramebuffer Get() const { return m_Framebuffer; }

    private:
        void RT_Create();

        Ref<VulkanDevice> m_Device;
        VulkanSpec m_Spec;

        VkFramebuffer m_Framebuffer;
    };
} // namespace fuujin