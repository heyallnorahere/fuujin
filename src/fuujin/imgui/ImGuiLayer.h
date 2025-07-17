#pragma once
#include "fuujin/core/Layer.h"

namespace fuujin {
    class ImGuiLayer : public Layer {
    public:
        ImGuiLayer();
        virtual ~ImGuiLayer() override;

        virtual void ProcessEvent(Event& event) override;

        virtual void PreUpdate(Duration delta) override;
        virtual void PostUpdate(Duration delta) override;
    };
} // namespace fuujin
