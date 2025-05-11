#pragma once
#include "fuujin/renderer/Swapchain.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanImage.h"
#include "fuujin/platform/vulkan/VulkanRenderPass.h"
#include "fuujin/platform/vulkan/VulkanFramebuffer.h"

#include "fuujin/core/Event.h"

namespace fuujin {
    class VulkanSwapchain : public Swapchain {
    public:
        VulkanSwapchain(Ref<View> view, Ref<VulkanDevice> device);
        virtual ~VulkanSwapchain() override;

        // this method is BOUND TO THE CURRENT THREAD
        // only call inside the render thread
        std::optional<uint32_t> RT_FindDeviceQueue();

        void Initialize(const VmaAllocator& allocator);
        void ProcessEvent(Event& event);

        virtual Ref<View> GetView() const override { return m_View; }
        virtual Ref<GraphicsDevice> GetDevice() const override { return m_Device; }
        Ref<VulkanDevice> GetVulkanDevice() const { return m_Device; }

        virtual ViewSize GetSize() const override { return { m_Extent.width, m_Extent.height }; }
        virtual void RequestResize(const ViewSize& viewSize) override { m_NewViewSize = viewSize; }

    private:
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
        uint32_t m_CurrentImage;

        VkExtent2D m_Extent;
        std::optional<ViewSize> m_NewViewSize;

        VmaAllocator m_Allocator;
        std::optional<uint32_t> m_RenderQueue;
    };
} // namespace fuujin