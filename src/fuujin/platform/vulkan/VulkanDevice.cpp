#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    VulkanDevice::VulkanDevice(Ref<VulkanInstance> instance, VkPhysicalDevice physicalDevice)
        : m_Instance(instance), m_PhysicalDevice(physicalDevice), m_Device(VK_NULL_HANDLE) {
        ZoneScoped;

        // ????
    }

    VulkanDevice::~VulkanDevice() {
        // todo: clean up logical device, if it was created
    }

    void VulkanDevice::GetProperties(Properties& props) const {
        ZoneScoped;

        VkPhysicalDeviceProperties2 properties{};
        GetProperties(properties);

        props.Name = properties.properties.deviceName;
        props.API = "Vulkan";

        switch (properties.properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            props.Type = DeviceType::Discrete;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            props.Type = DeviceType::CPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            props.Type = DeviceType::Integrated;
            break;
        default:
            props.Type = DeviceType::Other;
        }
    }

    void VulkanDevice::GetProperties(VkPhysicalDeviceProperties2& properties) const {
        ZoneScoped;

        if (vkGetPhysicalDeviceProperties2 != nullptr) {
            properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &properties);
        } else {
            vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties.properties);
        }
    }

    void VulkanDevice::GetFeatures(VkPhysicalDeviceFeatures2& features) const {
        ZoneScoped;

        if (vkGetPhysicalDeviceFeatures2 != nullptr) {
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &features);
        } else {
            vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &features.features);
        }
    }

    void VulkanDevice::GetMemoryProperties(VkPhysicalDeviceMemoryProperties& properties) const {
        ZoneScoped;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &properties);
    }
} // namespace fuujin