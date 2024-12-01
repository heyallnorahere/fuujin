#pragma once
#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanInstance.h"
#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanSwapchain.h"

namespace fuujin {
    struct ContextData;

    class VulkanContext : public GraphicsContext {
    public:
        static Ref<VulkanContext> Get(const std::optional<std::string>& deviceName = {});
        static VkAllocationCallbacks& GetAllocCallbacks();

        virtual ~VulkanContext() override;

        Ref<VulkanInstance> GetInstance() const;
        Ref<VulkanDevice> GetVulkanDevice(const std::optional<std::string>& deviceName = {}) const;

        virtual Ref<GraphicsDevice> GetDevice() const override;

    private:
        VulkanContext(const std::optional<std::string>& deviceName);

        void RT_LoadInstance() const;
        void RT_LoadDevice() const;

        void RT_QueryPresentQueue();
        void RT_CreateAllocator();

        void RT_EnumerateDevices(const std::optional<std::string>& deviceName);

        ContextData* m_Data;
    };
} // namespace fuujin