#pragma once
#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanInstance.h"

namespace fuujin {
    class VulkanDevice : public GraphicsDevice {
    public:
        VulkanDevice(Ref<VulkanInstance> instance, VkPhysicalDevice physicalDevice);
        virtual ~VulkanDevice() override;

        virtual void GetProperties(Properties& props) const override;

        void GetProperties(VkPhysicalDeviceProperties2& properties) const;
        void GetFeatures(VkPhysicalDeviceFeatures2& features) const;
        void GetMemoryProperties(VkPhysicalDeviceMemoryProperties& properties) const;

        Ref<VulkanInstance> GetInstance() const { return m_Instance; }

        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkDevice GetDevice() const { return m_Device; }

    private:
        Ref<VulkanInstance> m_Instance;
        VkPhysicalDevice m_PhysicalDevice;

        VkDevice m_Device;
    };
}