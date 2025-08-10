#pragma once
#include "fuujin/renderer/Framebuffer.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanImage.h"
#include "fuujin/platform/vulkan/VulkanRenderPass.h"
#include "fuujin/platform/vulkan/VulkanCommandQueue.h"
#include "fuujin/platform/vulkan/VulkanTexture.h"

namespace fuujin {
    class VulkanFramebuffer : public Framebuffer {
    public:
        struct VulkanSpec {
            Ref<VulkanRenderPass> RenderPass;
            std::vector<Ref<VulkanImage>> Attachments;
            VkExtent2D Extent;

            VkSampleCountFlagBits Samples;
            uint32_t Layers = 1;
            VkFramebufferCreateFlags Flags = 0;
        };

        VulkanFramebuffer(const Ref<VulkanDevice>& device, const Spec& spec,
                          const VmaAllocator& allocator, const Ref<VulkanRenderPass>& renderPass);

        VulkanFramebuffer(const Ref<VulkanDevice>& device, const VulkanSpec& spec);
        virtual ~VulkanFramebuffer() override;

        VkFramebuffer Get() const { return m_Framebuffer; }

        Ref<VulkanRenderPass> GetRenderPass() const { return m_VulkanSpec.RenderPass; }

        virtual uint32_t GetWidth() const override { return m_VulkanSpec.Extent.width; }
        virtual uint32_t GetHeight() const override { return m_VulkanSpec.Extent.height; }

        virtual uint32_t GetSamples() const override { return (uint32_t)m_VulkanSpec.Samples; }

        virtual uint32_t GetID() const override { return m_VulkanSpec.RenderPass->GetID(); }

        virtual void RT_BeginRender(CommandList& cmdList, const glm::vec4& clearColor) override;
        virtual void RT_EndRender(CommandList& cmdList) override;

        void RT_BeginRenderPass(VulkanCommandBuffer* buffer, const glm::vec4& clear) const;
        void RT_EndRenderPass(VulkanCommandBuffer* buffer) const;

        virtual const Spec& GetSpec() const override { return m_Spec; }

        virtual Ref<Texture> GetTexture(size_t index) const override;

        virtual Ref<Framebuffer> Recreate(uint32_t width, uint32_t height) const override;

    private:
        void RT_Create();

        const VmaAllocator* m_Allocator;
        Ref<VulkanDevice> m_Device;
        VulkanSpec m_VulkanSpec;

        Spec m_Spec;
        std::vector<Ref<VulkanTexture>> m_Textures;

        VkFramebuffer m_Framebuffer;
    };
} // namespace fuujin
