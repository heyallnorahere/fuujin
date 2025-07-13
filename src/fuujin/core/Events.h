#pragma once
#include "fuujin/core/Event.h"

#include "fuujin/core/Input.h"
#include "fuujin/core/Platform.h"
#include "fuujin/core/View.h"

namespace fuujin {
    class ViewResizedEvent : public Event {
    public:
        ViewResizedEvent(const ViewSize& size, uint64_t view)
            : Event(EventType::ViewResized), m_Size(size), m_View(view) {}

        const ViewSize& GetSize() const { return m_Size; }
        uint64_t GetView() const { return m_View; }

    private:
        ViewSize m_Size;
        uint64_t m_View;
    };

    class ViewFocusedEvent : public Event {
    public:
        ViewFocusedEvent(bool focused, uint64_t view)
            : Event(EventType::ViewFocused), m_Focused(focused), m_View(view) {}

        bool IsFocused() const { return m_Focused; }
        uint64_t GetView() const { return m_View; }

    private:
        bool m_Focused;
        uint64_t m_View;
    };

    class CursorEnteredEvent : public Event {
    public:
        CursorEnteredEvent(bool entered, uint64_t view)
            : Event(EventType::CursorEntered), m_Entered(entered), m_View(view) {}

        bool DidEnter() const { return m_Entered; }
        uint64_t GetView() const { return m_View; }

    private:
        bool m_Entered;
        uint64_t m_View;
    };

    class CursorPositionEvent : public Event {
    public:
        CursorPositionEvent(double x, double y, uint64_t view)
            : Event(EventType::CursorPosition), m_X(x), m_Y(y), m_View(view) {}

        double GetX() const { return m_X; }
        double GetY() const { return m_Y; }

        uint64_t GetView() const { return m_View; }

    private:
        double m_X, m_Y;
        uint64_t m_View;
    };

    class MouseButtonEvent : public Event {
    public:
        MouseButtonEvent(uint32_t button, bool pressed, uint64_t view)
            : Event(EventType::MouseButton), m_Button(button), m_Pressed(pressed), m_View(view) {}

        uint32_t GetButton() const { return m_Button; }
        bool IsPressed() const { return m_Pressed; }

        uint64_t GetView() const { return m_View; }

    private:
        uint32_t m_Button;
        bool m_Pressed;
        uint64_t m_View;
    };

    class ScrollEvent : public Event {
    public:
        ScrollEvent(double xOffset, double yOffset, uint64_t view)
            : Event(EventType::Scroll), m_XOffset(xOffset), m_YOffset(yOffset), m_View(view) {}

        double GetXOffset() const { return m_XOffset; }
        double GetYOffset() const { return m_YOffset; }

        uint64_t GetView() const { return m_View; }

    private:
        double m_XOffset, m_YOffset;
        uint64_t m_View;
    };

    class KeyEvent : public Event {
    public:
        KeyEvent(Key key, bool pressed, uint64_t view)
            : Event(EventType::Key), m_Key(key), m_Pressed(pressed), m_View(view) {}

        Key GetKey() const { return m_Key; }
        bool IsPressed() const { return m_Pressed; }

        uint64_t GetView() const { return m_View; }

    private:
        Key m_Key;
        bool m_Pressed;
        uint64_t m_View;
    };

    class CharEvent : public Event {
    public:
        CharEvent(uint32_t character, uint64_t view)
            : Event(EventType::Char), m_Character(character), m_View(view) {}

        uint32_t GetCharacter() const { return m_Character; }
        uint64_t GetView() const { return m_View; }

    private:
        uint32_t m_Character;
        uint64_t m_View;
    };

    class MonitorUpdateEvent : public Event {
    public:
        MonitorUpdateEvent(const MonitorInfo& monitor, uint32_t index)
            : Event(EventType::MonitorUpdate), m_Monitor(monitor), m_Index(index) {}

        const MonitorInfo& GetMonitor() const { return m_Monitor; }
        uint32_t GetIndex() const { return m_Index; }

    private:
        MonitorInfo m_Monitor;
        uint32_t m_Index;
    };
} // namespace fuujin