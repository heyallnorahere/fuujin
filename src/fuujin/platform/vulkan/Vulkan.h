#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <volk.h>
#include <vk_mem_alloc.h>

namespace fuujin {
    inline Version FromVulkanVersion(uint32_t version) {
        Version result;

        result.Major = VK_VERSION_MAJOR(version);
        result.Minor = VK_VERSION_MINOR(version);
        result.Patch = VK_VERSION_PATCH(version);

        return std::move(result);
    }

    inline uint32_t ToVulkanVersion(const Version& version) {
        return VK_MAKE_API_VERSION(0, version.Major, version.Minor, version.Patch);
    }
};