#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanDevice.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    VkQueueFlagBits VulkanDevice::ConvertQueueType(QueueType type) {
        ZoneScoped;

        switch (type) {
        case QueueType::Graphics:
            return VK_QUEUE_GRAPHICS_BIT;
        case QueueType::Transfer:
            return VK_QUEUE_TRANSFER_BIT;
        case QueueType::Compute:
            return VK_QUEUE_COMPUTE_BIT;
        default:
            return VK_QUEUE_FLAG_BITS_MAX_ENUM;
        }
    }

    VkQueueFlags VulkanDevice::ConvertQueueFlags(const std::vector<QueueType>& types) {
        ZoneScoped;

        VkQueueFlags flags = 0;
        for (auto type : types) {
            auto flag = ConvertQueueType(type);
            if (flag == VK_QUEUE_FLAG_BITS_MAX_ENUM) {
                continue;
            }

            flags |= flag;
        }

        return flags;
    }

    VulkanDevice::VulkanDevice(Ref<VulkanInstance> instance, VkPhysicalDevice physicalDevice)
        : m_Instance(instance), m_PhysicalDevice(physicalDevice), m_Device(VK_NULL_HANDLE),
          m_Initialized(false) {
        ZoneScoped;

        Renderer::Submit([&]() { RT_GetProperties(); });
    }

    VulkanDevice::~VulkanDevice() {
        ZoneScoped;

        if (m_Initialized) {
            VkDevice device = m_Device;
            Renderer::Submit(
                [device] { vkDestroyDevice(device, &VulkanContext::GetAllocCallbacks()); });
        }
    }

    void VulkanDevice::RT_GetProperties(VkPhysicalDeviceProperties2& properties) const {
        ZoneScoped;

        if (vkGetPhysicalDeviceProperties2 != nullptr) {
            properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &properties);
        } else {
            vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties.properties);
        }
    }

    void VulkanDevice::RT_GetFeatures(VkPhysicalDeviceFeatures2& features) const {
        ZoneScoped;

        if (vkGetPhysicalDeviceFeatures2 != nullptr) {
            features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &features);
        } else {
            vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &features.features);
        }
    }

    void VulkanDevice::RT_GetExtensions(std::unordered_set<std::string>& extensions) const {
        ZoneScoped;

        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> available(extensionCount);
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount,
                                             available.data());

        for (const auto& extension : available) {
            extensions.insert(extension.extensionName);
        }
    }

    void VulkanDevice::RT_GetMemoryProperties(VkPhysicalDeviceMemoryProperties& properties) const {
        ZoneScoped;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &properties);
    }

    void VulkanDevice::RT_GetQueueFamilies(std::vector<VkQueueFamilyProperties>& families) const {
        ZoneScoped;

        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &familyCount, nullptr);

        families.resize(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &familyCount, families.data());
    }

    VkSampleCountFlagBits VulkanDevice::RT_GetMaxSampleCount() const {
        ZoneScoped;

        VkPhysicalDeviceProperties2 properties;
        RT_GetProperties(properties);

        auto flags = properties.properties.limits.framebufferColorSampleCounts;
        for (uint32_t i = 6; i > 0; i--) {
            auto sampleCount = (VkSampleCountFlagBits)(1 << i);
            if ((flags & sampleCount) == sampleCount) {
                return sampleCount;
            }
        }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkFormatFeatureFlags VulkanDevice::RT_GetFormatFeatureFlags(VkFormat format,
                                                                VkImageTiling tiling) const {
        ZoneScoped;

        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &properties);

        switch (tiling) {
        case VK_IMAGE_TILING_LINEAR:
            return properties.linearTilingFeatures;
        case VK_IMAGE_TILING_OPTIMAL:
            return properties.optimalTilingFeatures;
        default:
            return 0;
        }
    }

    std::optional<VkFormat> VulkanDevice::RT_FindSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling,
        VkFormatFeatureFlags features) const {
        ZoneScoped;
        for (VkFormat format : candidates) {
            auto flags = RT_GetFormatFeatureFlags(format, tiling);
            if ((flags & features) == features) {
                return format;
            }
        }

        return {};
    }

    bool VulkanDevice::Initialize(void* next, size_t nextSize) {
        ZoneScoped;
        if (m_Initialized) {
            return true;
        }

        m_Initialized = true;

        void* nextBlock = nullptr;
        if (next != nullptr && nextSize != 0) {
            nextBlock = allocate(nextSize);
            std::memcpy(nextBlock, next, nextSize);
        }

        Renderer::Submit([this, nextBlock]() mutable { RT_Initialize(nextBlock); });

        return true;
    }

    void VulkanDevice::RT_Initialize(void* next) {
        ZoneScoped;

        VkPhysicalDeviceFeatures2 features{};
        uint32_t api = m_Instance->GetSpec().API;

        bool advancedFeatures = false;
        std::vector<void*> blocks = { next };

        if (api >= VK_API_VERSION_1_1) {
            auto features11 = (VkPhysicalDeviceVulkan11Features*)allocate(
                sizeof(VkPhysicalDeviceVulkan11Features));
            blocks.push_back(features11);

            FUUJIN_DEBUG("Enabling Vulkan 1.1 features");
            features11->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features.pNext = features11;

            if (api >= VK_API_VERSION_1_2) {
                auto features12 = (VkPhysicalDeviceVulkan12Features*)allocate(
                    sizeof(VkPhysicalDeviceVulkan12Features));
                blocks.push_back(features12);

                FUUJIN_DEBUG("Enabling Vulkan 1.2 features");
                features12->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                features11->pNext = features12;

                if (api >= VK_API_VERSION_1_3) {
                    auto features13 = (VkPhysicalDeviceVulkan13Features*)allocate(
                        sizeof(VkPhysicalDeviceVulkan13Features));
                    blocks.push_back(features13);

                    FUUJIN_DEBUG("Enabling Vulkan 1.3 features");
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

        RT_GetFeatures(features);
        RT_SelectQueues();

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
            FUUJIN_DEBUG("Creating device queue on family {}", index);
        }

        std::unordered_set<std::string> availableExtensions;
        RT_GetExtensions(availableExtensions);

        spdlog::enable_backtrace(availableExtensions.size());
        for (const auto& extension : availableExtensions) {
            FUUJIN_TRACE("Device extension: {}", extension.c_str());
        }

        std::vector<const char*> extensions;
        for (const auto& extension : m_Spec.Extensions) {
            if (!availableExtensions.contains(extension)) {
                s_Logger.dump_backtrace();

                FUUJIN_ERROR("Extension " + extension + " is not present on this device!");
            }

            extensions.push_back(extension.c_str());
        }

        spdlog::disable_backtrace();

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
            FUUJIN_CRITICAL("Failed to acquire device!");

            return;
        }

        FUUJIN_INFO("Successfully acquired Vulkan device: {}", m_Properties.Name.c_str());
        for (void* block : blocks) {
            freemem(block);
        }
    }

    static std::optional<uint32_t> FindQueueFamily(
        QueueType type, const std::vector<VkQueueFamilyProperties>& families) {
        ZoneScoped;

        static const auto relevantFlags = VulkanDevice::ConvertQueueFlags(
            { QueueType::Graphics, QueueType::Transfer, QueueType::Compute });

        std::string queueName;
        switch (type) {
        case QueueType::Graphics:
            queueName = "graphics";
            break;
        case QueueType::Transfer:
            queueName = "transfer";
            break;
        case QueueType::Compute:
            queueName = "compute";
            break;
        default:
            queueName = "other";
            break;
        }

        FUUJIN_DEBUG("Finding {} family...", queueName.c_str());
        auto flag = VulkanDevice::ConvertQueueType(type);

        std::optional<uint32_t> family;
        for (uint32_t i = 0; i < families.size(); i++) {
            const auto& properties = families[i];

            if ((properties.queueFlags & flag) != flag) {
                continue;
            }

            if (!family.has_value()) {
                family = i;
            }

            if ((properties.queueFlags & relevantFlags) == flag) {
                FUUJIN_DEBUG("Found exclusive {} queue: {}", queueName.c_str(), i);
                return i;
            }
        }

        if (family.has_value()) {
            FUUJIN_DEBUG("Found non-exclusive {} queue: {}", queueName.c_str(), family.value());
        } else {
            FUUJIN_WARN("No {} family found!", queueName.c_str());
        }

        return family;
    }

    void VulkanDevice::RT_SelectQueues() {
        ZoneScoped;

        std::vector<VkQueueFamilyProperties> families;
        RT_GetQueueFamilies(families);

        for (auto type : m_Spec.RequestedQueues) {
            auto family = FindQueueFamily(type, families);
            if (family.has_value()) {
                m_Queues[type] = family.value();
            }
        }
    }

    void VulkanDevice::RT_GetProperties() {
        ZoneScoped;

        VkPhysicalDeviceProperties2 properties{};
        RT_GetProperties(properties);

        m_Properties.Name = properties.properties.deviceName;
        m_Properties.DriverVersion = FromVulkanVersion(properties.properties.driverVersion);

        m_Properties.API = "Vulkan";
        m_Properties.APIVersion = FromVulkanVersion(properties.properties.apiVersion);

        switch (properties.properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            m_Properties.Type = GraphicsDeviceType::Discrete;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            m_Properties.Type = GraphicsDeviceType::CPU;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            m_Properties.Type = GraphicsDeviceType::Integrated;
            break;
        default:
            m_Properties.Type = GraphicsDeviceType::Other;
        }
    }
} // namespace fuujin