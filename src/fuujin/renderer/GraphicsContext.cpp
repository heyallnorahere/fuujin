#include "fuujinpch.h"
#include "fuujin/renderer/GraphicsContext.h"

#ifdef FUUJIN_PLATFORM_vulkan
#include "fuujin/platform/vulkan/VulkanContext.h"
#endif

namespace fuujin {
    Ref<GraphicsContext> GraphicsContext::Get(const std::optional<std::string>& deviceName) {
#ifdef FUUJIN_PLATFORM_vulkan
        return VulkanContext::Get(deviceName);
#else
        return nullptr;
#endif
    }
} // namespace fuujin