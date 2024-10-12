#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanDevice.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    VulkanDevice::VulkanDevice(Ref<VulkanInstance> instance, VkPhysicalDevice physicalDevice)
        : m_Instance(instance), m_PhysicalDevice(physicalDevice), m_Device(VK_NULL_HANDLE),
          m_Initialized(false) {
        ZoneScoped;

        // ????
    }

    VulkanDevice::~VulkanDevice() {
        ZoneScoped;

        VkDevice device = m_Device;
        Renderer::Submit(
            [device] { vkDestroyDevice(device, &VulkanContext::GetAllocCallbacks()); });
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

    void VulkanDevice::GetExtensions(std::vector<VkExtensionProperties>& extensions) const {
        ZoneScoped;

        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);

        extensions.resize(extensionCount);
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount,
                                             extensions.data());
    }

    void VulkanDevice::GetMemoryProperties(VkPhysicalDeviceMemoryProperties& properties) const {
        ZoneScoped;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &properties);
    }

    bool VulkanDevice::Initialize(const Spec& spec, void* next, size_t nextSize) {
        ZoneScoped;
        if (m_Initialized) {
            return true;
        }

        m_Spec = spec;
        m_Initialized = true;

        void* nextBlock = allocate(nextSize);
        std::memcpy(nextBlock, next, nextSize);

        Renderer::Submit([&]() {
            SelectQueues();
            DoInitialize(nextBlock);
        });

        return true;
    }

    void VulkanDevice::DoInitialize(void* next) {
        ZoneScoped;

        VkPhysicalDeviceFeatures2 features{};
        uint32_t api = m_Instance->GetSpec().API;

        bool advancedFeatures = false;
        std::vector<void*> blocks = { next };

        if (api >= VK_API_VERSION_1_1) {
            auto features11 = (VkPhysicalDeviceVulkan11Features*)allocate(
                sizeof(VkPhysicalDeviceVulkan11Features));
            blocks.push_back(features11);

            features11->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features.pNext = features11;

            if (api >= VK_API_VERSION_1_2) {
                auto features12 = (VkPhysicalDeviceVulkan12Features*)allocate(
                    sizeof(VkPhysicalDeviceVulkan12Features));
                blocks.push_back(features12);

                features12->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                features11->pNext = features12;

                if (api >= VK_API_VERSION_1_3) {
                    auto features13 = (VkPhysicalDeviceVulkan13Features*)allocate(
                        sizeof(VkPhysicalDeviceVulkan13Features));
                    blocks.push_back(features13);

                    features13->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                    features13->pNext = next;
                    features12->pNext = features13;
                } else {
                    features12->pNext = next;
                }
            } else {
                features11->pNext = next;
            }

            advancedFeatures = true;
        }

        std::unordered_set<uint32_t> queues(m_Spec.AdditionalQueues);
        for (auto [type, index] : m_Queues) {
            queues.insert(index);
        }

        std::vector<VkDeviceQueueCreateInfo> queueInfo;
        float priority = 1.f;

        for (uint32_t index : queues) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = index;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &priority;

            queueInfo.push_back(std::move(queueCreateInfo));
        }

        std::vector<VkExtensionProperties> availableExtensions;
        GetExtensions(availableExtensions);

        std::vector<const char*> extensions;
        for (const auto& extension : m_Spec.Extensions) {
            const char* allocated = nullptr;
            for (const auto& available : availableExtensions) {
                if (available.extensionName == extension) {
                    allocated = available.extensionName;
                    break;
                }
            }

            if (allocated == nullptr) {
                throw std::runtime_error("Extension " + extension +
                                         " is not present on this device!");
            }

            extensions.push_back(allocated);
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = advancedFeatures ? &features : next;
        createInfo.queueCreateInfoCount = (uint32_t)queueInfo.size();
        createInfo.pQueueCreateInfos = queueInfo.data();
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.pEnabledFeatures = advancedFeatures ? nullptr : &features.features;

        if (vkCreateDevice(m_PhysicalDevice, &createInfo, &VulkanContext::GetAllocCallbacks(),
                           &m_Device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire device!");
        }

        for (void* block : blocks) {
            freemem(block);
        }
    }

    void VulkanDevice::SelectQueues() {
        ZoneScoped;

        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &familyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &familyCount,
                                                 queueFamilies.data());
    }
} // namespace fuujin