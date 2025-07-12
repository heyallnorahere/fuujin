#pragma once
#include "fuujin/core/Event.h"

#include "fuujin/core/View.h"

namespace fuujin {
    class FramebufferResizedEvent : public Event {
    public:
        FramebufferResizedEvent(const ViewSize& size, uint64_t window)
            : Event(EventType::FramebufferResized), m_Size(size), m_Window(window) {}

        const ViewSize& GetSize() const { return m_Size; }
        uint64_t GetWindow() const { return m_Window; }

    private:
        ViewSize m_Size;
        uint64_t m_Window;
    };
} // namespace fuujin