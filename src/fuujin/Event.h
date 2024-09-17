#pragma once

#include <functional>
#include <type_traits>

namespace fuujin {
    enum class EventType {
        FramebufferResized
    };

    class Event {
    public:
        Event(EventType type) : m_Type(type), m_Processed(false) {}
        ~Event() = default;

        EventType GetType() const { return m_Type; }

        bool IsProcessed() const { return m_Processed; }
        void SetProcessed(bool doSet = true) { m_Processed |= doSet; }

    private:
        bool m_Processed;
        EventType m_Type;
    };

    class EventDispatcher {
    public:
        EventDispatcher(Event& event) : m_Event(&event), m_DidProcess(false) {}
        ~EventDispatcher() = default;

        template <typename _Ty>
        void Dispatch(EventType type, const std::function<bool(const _Ty&)>& handler) {
            static_assert(std::is_base_of_v<Event, _Ty>, "Parameter must be an event!");

            if (m_Event->IsProcessed()) {
                return;
            }

            if (type == m_Event->GetType()) {
                auto& event = *(_Ty*)m_Event;
                bool processed = handler(event);

                m_DidProcess |= processed;
                if (!m_Event->IsProcessed()) {
                    m_Event->SetProcessed(processed);
                }
            }
        }

    private:
        Event* m_Event;
        bool m_DidProcess;
    };
} // namespace fuujin