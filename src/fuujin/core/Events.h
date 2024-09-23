#pragma once
#include "fuujin/core/Event.h"

#include "fuujin/core/View.h"

namespace fuujin {
    class FramebufferResizedEvent : public Event {
    public:
        FramebufferResizedEvent(const ViewSize& size)
            : Event(EventType::FramebufferResized), m_Size(size) {}

        const ViewSize& GetSize() const { return m_Size; }

    private:
        ViewSize m_Size;
    };
} // namespace fuujin