#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/renderer/Swapchain.h"
#include "fuujin/renderer/CommandQueue.h"

namespace fuujin {
    class GraphicsContext : public RefCounted {
    public:
        static Ref<GraphicsContext> Get(const std::optional<std::string>& deviceName = {});

        virtual Ref<GraphicsDevice> GetDevice() const = 0;
        virtual Ref<Swapchain> GetSwapchain() const = 0;
        virtual Ref<CommandQueue> GetQueue(QueueType type) const = 0;

        virtual Ref<Fence> CreateFence(bool signaled = false) const = 0;
        virtual Ref<RefCounted> CreateSemaphore() const = 0;
    };
} // namespace fuujin