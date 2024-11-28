#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanSwapchain.h"

#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    VulkanSwapchain::VulkanSwapchain(Ref<View> view, Ref<VulkanDevice> device)
        : m_View(view), m_Device(m_Device) {
        ZoneScoped;

        m_Swapchain = VK_NULL_HANDLE;
        m_Surface = (VkSurfaceKHR)view->CreateVulkanSurface(device->GetInstance()->GetInstance());
    }

    VulkanSwapchain::~VulkanSwapchain() {
        ZoneScoped;
        auto callbacks = &VulkanContext::GetAllocCallbacks();

        if (m_Surface != VK_NULL_HANDLE) {
            auto instance = m_Device->GetInstance();
            vkDestroySurfaceKHR(instance->GetInstance(), m_Surface, callbacks);
        }
    }
} // namespace fuujin