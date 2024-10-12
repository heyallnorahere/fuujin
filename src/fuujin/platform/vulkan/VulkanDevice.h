#pragma once
#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanInstance.h"

namespace fuujin {
    enum class QueueType {
        Graphics,
        Transfer,
        Compute
    };

    class VulkanDevice : public GraphicsDevice {
    public:
        struct Spec {
            std::vector<std::string> Extensions;
            std::unordered_set<uint32_t> AdditionalQueues;
        };

        VulkanDevice(Ref<VulkanInstance> instance, VkPhysicalDevice physicalDevice);
        virtual ~VulkanDevice() override;

        virtual void GetProperties(Properties& props) const override;

        void GetProperties(VkPhysicalDeviceProperties2& properties) const;
        void GetFeatures(VkPhysicalDeviceFeatures2& features) const;
        void GetExtensions(std::vector<VkExtensionProperties>& extensions) const;
        void GetMemoryProperties(VkPhysicalDeviceMemoryProperties& properties) const;

        Ref<VulkanInstance> GetInstance() const { return m_Instance; }

        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkDevice GetDevice() const { return m_Device; }

        const Spec& GetSpec() const { return m_Spec; }

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