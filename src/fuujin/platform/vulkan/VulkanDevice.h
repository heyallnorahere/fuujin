#pragma once
#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanInstance.h"

namespace fuujin {
    class VulkanDevice : public GraphicsDevice {
    public:
        struct Spec {
            std::unordered_set<std::string> Extensions;
            std::unordered_set<uint32_t> AdditionalQueues;
            std::unordered_set<QueueType> RequestedQueues;
        };

        static VkQueueFlagBits ConvertQueueType(QueueType type);
        static VkQueueFlags ConvertQueueFlags(const std::vector<QueueType>& types);

        VulkanDevice(Ref<VulkanInstance> instance, VkPhysicalDevice physicalDevice);
        virtual ~VulkanDevice() override;

        virtual void GetProperties(Properties& props) const override { props = m_Properties; }

        virtual void Wait() const override;

        // note: these should preferably be called from inside the render thread
        void RT_GetProperties(VkPhysicalDeviceProperties2& properties) const;
        void RT_GetFeatures(VkPhysicalDeviceFeatures2& features) const;
        void RT_GetExtensions(std::unordered_set<std::string>& extensions) const;
        void RT_GetMemoryProperties(VkPhysicalDeviceMemoryProperties& properties) const;
        void RT_GetQueueFamilies(std::vector<VkQueueFamilyProperties>& families) const;

        VkSampleCountFlagBits RT_GetMaxSampleCount() const;
        VkFormatFeatureFlags RT_GetFormatFeatureFlags(VkFormat format, VkImageTiling tiling) const;
        std::optional<VkFormat> RT_FindSupportedFormat(const std::vector<VkFormat>& candidates,
                                                       VkImageTiling tiling,
                                                       VkFormatFeatureFlags features) const;

        Ref<VulkanInstance> GetInstance() const { return m_Instance; }

        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkDevice GetDevice() const { return m_Device; }

        Spec& GetSpec() { return m_Spec; }
        const Spec& GetSpec() const { return m_Spec; }
        const std::unordered_map<QueueType, uint32_t>& GetQueues() const { return m_Queues; }

        bool Initialize(void* next = nullptr, size_t nextSize = 0);

        template <typename _Ty>
        VkSharingMode GetSharingMode(const _Ty& ownership, std::vector<uint32_t>& indices) const {
            ZoneScoped;

            std::unordered_set<uint32_t> queueIndices;
            for (QueueType type : ownership) {
                if (!m_Queues.contains(type)) {
                    continue;
                }

                queueIndices.insert(m_Queues.at(type));
            }

            if (queueIndices.size() <= 1) {
                return VK_SHARING_MODE_EXCLUSIVE;
            } else {
                indices.resize(queueIndices.size());
                std::copy(queueIndices.begin(), queueIndices.end(), indices.begin());

                return VK_SHARING_MODE_CONCURRENT;
            }
        }

    private:
        void RT_Initialize(void* next);
        void RT_SelectQueues();
        void RT_GetDeviceProperties();

        Ref<VulkanInstance> m_Instance;
        VkPhysicalDevice m_PhysicalDevice;

        VkDevice m_Device;

        Spec m_Spec;
        bool m_Initialized;
        std::unordered_map<QueueType, uint32_t> m_Queues;
        Properties m_Properties;
    };
} // namespace fuujin