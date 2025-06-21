#pragma once
#include "fuujin/renderer/Framebuffer.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanImage.h"
#include "fuujin/platform/vulkan/VulkanRenderPass.h"
#include "fuujin/platform/vulkan/VulkanCommandQueue.h"

namespace fuujin {
    class VulkanFramebuffer : public Framebuffer {
    public:
        struct VulkanSpec {
            Ref<VulkanRenderPass> RenderPass;
            std::vector<Ref<VulkanImage>> Attachments;
            VkExtent2D Extent;

            uint32_t Layers = 1;
            VkFramebufferCreateFlags Flags = 0;

            std::optional<std::function<Ref<VulkanSemaphore>()>> SemaphoreCallback;
        };

        VulkanFramebuffer(Ref<VulkanDevice> device, const VulkanSpec& spec);
        virtual ~VulkanFramebuffer() override;

        VkFramebuffer Get() const { return m_Framebuffer; }

        Ref<VulkanRenderPass> GetRenderPass() const { return m_Spec.RenderPass; }

        virtual uint32_t GetWidth() const override { return m_Spec.Extent.width; }
        virtual uint32_t GetHeight() const override { return m_Spec.Extent.height; }

        virtual void AddImageDependency(CommandList& cmdList) override;

        virtual void RT_BeginRender(CommandList& cmdList) override;
        virtual void RT_EndRender(CommandList& cmdList) override;

        void RT_BeginRenderPass(VulkanCommandBuffer* buffer, const glm::vec4& clear) const;
        void RT_EndRenderPass(VulkanCommandBuffer* buffer) const;

    private:
        void RT_Create();

        Ref<VulkanSemaphore> GetSemaphore() const;

        Ref<VulkanDevice> m_Device;
        VulkanSpec m_Spec;

        VkFramebuffer m_Framebuffer;
    };
} // namespace fuujin