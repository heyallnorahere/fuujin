#pragma once
#include "fuujin/renderer/Renderer.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    class VulkanRenderer : public RendererAPI {
    public:
        VulkanRenderer(Ref<VulkanDevice> device);
        ~VulkanRenderer();

        virtual void RT_RenderIndexed(CommandList& cmdlist, Ref<DeviceBuffer> vertices,
                                      Ref<DeviceBuffer> indices, Ref<Pipeline> pipeline,
                                      uint32_t indexCount,
                                      const Buffer& pushConstants = Buffer()) const override;

        virtual void RT_SetViewport(CommandList& cmdlist, Ref<RenderTarget> target) const override;

    private:
        Ref<VulkanDevice> m_Device;
    };
} // namespace fuujin