#pragma once

namespace fuujin {
    class Waitable {
    public:
        virtual ~Waitable() = default;

        template <typename _Rep = std::chrono::seconds::rep,
                  typename _Period = std::chrono::seconds::period>
        void Wait(const std::optional<std::chrono::duration<_Rep, _Period>>& timeout = {}) {
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
    };
} // namespace fuujin