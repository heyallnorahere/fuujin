#pragma once
#include "fuujin/renderer/Swapchain.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

#include "fuujin/core/Event.h"

namespace fuujin {
    class VulkanSwapchain : public Swapchain {
    public:
        VulkanSwapchain(Ref<View> view, Ref<VulkanDevice> device);
        virtual ~VulkanSwapchain() override;

        std::optional<uint32_t> FindDeviceQueue() const;
        bool Initialize(VmaAllocator allocator);

        void ProcessEvent(Event& event);

        virtual Ref<View> GetView() const override { return m_View; }
        virtual Ref<GraphicsDevice> GetDevice() const override { return m_Device; }
        Ref<VulkanDevice> GetVulkanDevice() const { return m_Device; }

        virtual ViewSize GetSize() const override { return { m_Extent.width, m_Extent.height }; }

    private:
        VkSurfaceKHR Create(const std::optional<VkExtent2D>& requested);

        Ref<View> m_View;
        Ref<VulkanDevice> m_Device;

        VkSwapchainKHR m_Swapchain;
        VkSurfaceKHR m_Surface;

        std::optional<VkExtent2D> m_RequestedExtent;
        VkExtent2D m_Extent;
    };
} // namespace fuujin