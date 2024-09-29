#pragma once
#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/platform/vulkan/Vulkan.h"

namespace fuujin {
    struct ContextData;

    class VulkanContext : public GraphicsContext {
    public:
        static Ref<VulkanContext> Get(const std::optional<std::string>& deviceName = {});
        static VkAllocationCallbacks& GetAllocCallbacks();

        virtual ~VulkanContext() override;

    private:
        VulkanContext(const std::optional<std::string>& deviceName);

        ContextData* m_Data;
    };
} // namespace fuujin