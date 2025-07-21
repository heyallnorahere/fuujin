#pragma once
#include "fuujin/core/View.h"

#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/renderer/Framebuffer.h"

namespace fuujin {
    class Swapchain : public RenderTarget {
    public:
        virtual ~Swapchain() = default;

        virtual RenderTargetType GetType() const override { return RenderTargetType::Swapchain; }

        virtual Ref<View> GetView() const = 0;
        virtual Ref<GraphicsDevice> GetDevice() const = 0;

        virtual void RequestResize(const ViewSize& viewSize) = 0;

        virtual uint32_t GetImageIndex() const = 0;
        virtual uint32_t GetImageCount() const = 0;
        virtual Ref<Framebuffer> GetFramebuffer(uint32_t index) const = 0;

        virtual void RT_AcquireImage() = 0;
        virtual void RT_Present() = 0;
    };
} // namespace fuujin
