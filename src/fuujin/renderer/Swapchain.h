#pragma once
#include "fuujin/core/View.h"

#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/renderer/CommandQueue.h"

namespace fuujin {
    class Swapchain : public RefCounted {
    public:
        virtual ~Swapchain() = default;

        virtual Ref<View> GetView() const = 0;
        virtual Ref<GraphicsDevice> GetDevice() const = 0;

        virtual ViewSize GetSize() const = 0;
        virtual void RequestResize(const ViewSize& viewSize) = 0;

        virtual void RT_AcquireImage() = 0;
        virtual void RT_Present(Ref<CommandQueue> queue, CommandList& cmdList) = 0;
    };
} // namespace fuujin