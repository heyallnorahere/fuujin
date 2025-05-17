#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/GraphicsDevice.h"

namespace fuujin {
    class Fence : public RefCounted {
    public:
        virtual void Reset() = 0;

        template <typename _Rep = std::chrono::seconds::rep,
                  typename _Period = std::chrono::seconds::period>
        void Wait(const std::optional<std::chrono::duration<_Rep, _Period>>& timeout = {}) const {
            ZoneScoped;

            uint64_t timeoutMs = std::numeric_limits<uint64_t>::max();
            if (timeout.has_value()) {
                timeoutMs = std::chrono::duration_cast<std::chrono::duration<uint64_t, std::milli>>(
                                timeout.value())
                                .count();
            }

            Wait(timeoutMs);
        }

        virtual void Wait(uint64_t timeout) const = 0;

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