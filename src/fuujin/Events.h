#pragma once
#include "fuujin/Event.h"

#include "fuujin/View.h"

namespace fuujin {
    class FramebufferResizedEvent : public Event {
    public:
        FramebufferResizedEvent(const ViewSize& size)
            : Event(EventType::FramebufferResized), m_Size(size) {}

        const ViewSize& GetSize() { return m_Size; }

    private:
        ViewSize m_Size;
    };
} // namespace fuujin