#pragma once
#include "fuujin/renderer/Swapchain.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanImage.h"
#include "fuujin/platform/vulkan/VulkanRenderPass.h"
#include "fuujin/platform/vulkan/VulkanFramebuffer.h"
#include "fuujin/platform/vulkan/VulkanCommandQueue.h"

#include "fuujin/core/Event.h"

namespace fuujin {
    class VulkanSwapchain : public Swapchain {
    public:
        VulkanSwapchain(Ref<View> view, Ref<VulkanDevice> device);
        virtual ~VulkanSwapchain() override;

        std::optional<uint32_t> RT_FindDeviceQueue();
        void RT_Initialize(VmaAllocator allocator);

        void ProcessEvent(Event& event);

        virtual Ref<View> GetView() const override { return m_View; }
        virtual Ref<GraphicsDevice> GetDevice() const override { return m_Device; }
        Ref<VulkanDevice> GetVulkanDevice() const { return m_Device; }

        virtual uint32_t GetWidth() const override { return m_Extent.width; }
        virtual uint32_t GetHeight() const override { return m_Extent.height; }

        virtual uint32_t GetID() const override { return m_RenderPass->GetID(); }

        virtual void RequestResize(const ViewSize& viewSize) override;

        virtual uint32_t GetImageIndex() const override { return m_CurrentImage; }
        virtual uint32_t GetImageCount() const override { return (uint32_t)m_Framebuffers.size(); }

        virtual Ref<Framebuffer> GetFramebuffer(uint32_t index) const override {
            return m_Framebuffers[index];
        }

        virtual size_t GetSyncFrameCount() const override;

        virtual Ref<Fence> GetCurrentFence() const override { return m_Sync[m_SyncFrame].Fence; }

        Ref<VulkanRenderPass> GetRenderPass() const { return m_RenderPass; }

        virtual void RT_BeginRender(CommandList& cmdList) override;
        virtual void RT_EndRender(CommandList& cmdList) override;
        virtual void RT_EndFrame() override;

        virtual void RT_AcquireImage() override;
        virtual void RT_Present() override;

    private:
        struct FrameSync {
            Ref<VulkanFence> Fence;
            Ref<VulkanSemaphore> ImageAvailable, RenderComplete;
        };

        void RT_Invalidate();
        VkSwapchainKHR RT_Create();

        VkSurfaceFormatKHR RT_FindSurfaceFormat() const;
        VkPresentModeKHR RT_FindPresentMode() const;

        void RT_CreateColorBuffer(VkSampleCountFlagBits samples);
        void RT_CreateDepthBuffer(VkSampleCountFlagBits samples);

        Ref<View> m_View;
        Ref<VulkanDevice> m_Device;
        Ref<VulkanRenderPass> m_RenderPass;

        VkSwapchainKHR m_Swapchain;
        VkSurfaceKHR m_Surface;
        VkFormat m_ImageFormat;

        Ref<VulkanImage> m_Color, m_Depth;
        bool m_DepthHasStencil;

        std::vector<Ref<VulkanFramebuffer>> m_Framebuffers;
        std::vector<Ref<VulkanFence>> m_ImageFences;
        uint32_t m_CurrentImage;

        size_t m_SyncFrame;
        std::vector<FrameSync> m_Sync;

        VkExtent2D m_Extent;
        std::optional<ViewSize> m_NewViewSize;

        VmaAllocator m_Allocator;
        std::optional<uint32_t> m_PresentFamily;
        VkQueue m_PresentQueue;
    };
} // namespace fuujin