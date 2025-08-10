#include "fuujinpch.h"
#include "fuujin/renderer/RenderLabel.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    RenderLabel::RenderLabel() : m_Label(), m_Pushed(false) {}

    RenderLabel::~RenderLabel() {
        if (m_Pushed) {
            Renderer::PopRenderLabel();
        }
    }

    RenderLabel::RenderLabel(const std::string& label) : m_Label(label), m_Pushed(true) {
        if (!Renderer::PushRenderLabel(label)) {
            FUUJIN_WARN("Failed to push render label: {}", label.c_str());
        }
    }

    RenderLabel::RenderLabel(RenderLabel&& other) {
        m_Label = other.m_Label;

        m_Pushed = other.m_Pushed;
        other.m_Pushed = false;
    }

    RenderLabel& RenderLabel::operator=(RenderLabel&& other) {
        m_Label = other.m_Label;

        m_Pushed = other.m_Pushed;
        other.m_Pushed = false;

        return *this;
    }
}; // namespace fuujin
