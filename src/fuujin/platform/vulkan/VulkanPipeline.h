#pragma once
#include "fuujin/renderer/Pipeline.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanShader.h"

namespace fuujin {
    class VulkanPipeline : public Pipeline {
    public:
        VulkanPipeline(Ref<VulkanDevice> device, const Spec& spec);
        virtual ~VulkanPipeline() override;

        virtual const Spec& GetSpec() const override { return m_Spec; }
        VkPipeline GetPipeline() const { return m_Pipeline; }

    private:
        void RT_Create();
        void RT_CreateComputePipeline();
        void RT_CreateGraphicsPipeline();

        VkPipeline m_Pipeline;

        Spec m_Spec;
        Ref<VulkanDevice> m_Device;
    };
} // namespace fuujin