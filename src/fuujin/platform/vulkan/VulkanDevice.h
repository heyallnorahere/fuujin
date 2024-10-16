#pragma once
#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanInstance.h"

namespace fuujin {
    class VulkanDevice : public GraphicsDevice {
    public:
        struct Spec {
            std::vector<std::string> Extensions;
            std::unordered_set<uint32_t> AdditionalQueues;
            std::unordered_set<QueueType> RequestedQueues;
        };

        static VkQueueFlagBits ConvertQueueType(QueueType type);
        static VkQueueFlags ConvertQueueFlags(const std::vector<QueueType>& types);

        VulkanDevice(Ref<VulkanInstance> instance, VkPhysicalDevice physicalDevice);
        virtual ~VulkanDevice() override;

        virtual void GetProperties(Properties& props) const override;

        void GetProperties(VkPhysicalDeviceProperties2& properties) const;
        void GetFeatures(VkPhysicalDeviceFeatures2& features) const;
        void GetExtensions(std::unordered_set<std::string>& extensions) const;
        void GetMemoryProperties(VkPhysicalDeviceMemoryProperties& properties) const;
        void GetQueueFamilies(std::vector<VkQueueFamilyProperties>& families) const;

        Ref<VulkanInstance> GetInstance() const { return m_Instance; }

        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkDevice GetDevice() const { return m_Device; }

        const Spec& GetSpec() const { return m_Spec; }
        const std::unordered_map<QueueType, uint32_t>& GetQueues() const { return m_Queues; }

        bool Initialize(const Spec& spec, void* next = nullptr, size_t nextSize = 0);

    private:
        void DoInitialize(void* next);
        void SelectQueues();

        Ref<VulkanInstance> m_Instance;
        VkPhysicalDevice m_PhysicalDevice;

        VkDevice m_Device;
        Spec m_Spec;
        bool m_Initialized;
        std::unordered_map<QueueType, uint32_t> m_Queues;
    };
} // namespace fuujin