#pragma once

namespace fuujin {
    class Event;

    class Layer {
    public:
        Layer() = default;
        virtual ~Layer() = default;

        Layer(const Layer&) = delete;
        Layer& operator=(const Layer&) = delete;

        virtual void ProcessEvent(Event& event) {}
        virtual void Update(Duration delta) {}

        virtual void PreUpdate(Duration delta) {}
        virtual void PostUpdate(Duration delta) {}
    };
} // namespace fuujin
