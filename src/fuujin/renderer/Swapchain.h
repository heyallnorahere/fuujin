#pragma once
#include "fuujin/core/View.h"
#include "fuujin/renderer/GraphicsDevice.h"

namespace fuujin {
    class Swapchain : public RefCounted {
    public:
        virtual ~Swapchain() = default;

        virtual Ref<View> GetView() const = 0;
        virtual Ref<GraphicsDevice> GetDevice() const = 0;

        virtual ViewSize GetSize() const = 0;
        virtual void RequestResize(const ViewSize& viewSize) = 0;
    };
} // namespace fuujin