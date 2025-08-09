#pragma once
#include "fuujin/core/Ref.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    class VulkanRenderPass : public RefCounted {
    public:
        VulkanRenderPass(Ref<VulkanDevice> device, const VkRenderPassCreateInfo* createInfo);
        virtual ~VulkanRenderPass() override;

        VkRenderPass Get() const { return m_RenderPass; }
        uint32_t GetID() const { return m_ID; }

    private:
        void RT_Create();

        VkRenderPassCreateInfo* m_CreateInfo;
        VkRenderPass m_RenderPass;
        uint32_t m_ID;

        Ref<VulkanDevice> m_Device;
    };
} // namespace fuujin
