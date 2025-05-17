#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/core/Utils.h"

#include "fuujin/renderer/GraphicsDevice.h"

namespace fuujin {
    class Fence : public RefCounted, public Waitable {
    public:
        virtual void Reset() = 0;
        
        virtual bool RT_IsReady() const = 0;
    };

    enum class SemaphoreUsage { Wait, Signal };

    class CommandList {
    public:
        virtual ~CommandList() = default;

        virtual void RT_Begin() = 0;
        virtual void RT_End() = 0;
        virtual void RT_Reset() = 0;

        virtual void AddSemaphore(Ref<RefCounted> semaphore, SemaphoreUsage usage) = 0;
    };

    class CommandQueue : public RefCounted {
    public:
        virtual QueueType GetType() const = 0;

        virtual CommandList& RT_Get() = 0;
        virtual void RT_Submit(CommandList& cmdList, Ref<Fence> fence = {}) = 0;

        virtual void Wait() const = 0;
    };
} // namespace fuujin