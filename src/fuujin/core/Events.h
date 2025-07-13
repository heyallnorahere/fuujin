#pragma once
#include "fuujin/core/Event.h"

#include "fuujin/core/View.h"

namespace fuujin {
    class FramebufferResizedEvent : public Event {
    public:
        FramebufferResizedEvent(const ViewSize& size, uint64_t view)
            : Event(EventType::FramebufferResized), m_Size(size), m_View(view) {}

        const ViewSize& GetSize() const { return m_Size; }
        uint64_t GetView() const { return m_View; }

    private:
        ViewSize m_Size;
        uint64_t m_View;
    };
} // namespace fuujin