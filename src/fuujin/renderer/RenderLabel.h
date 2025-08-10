#pragma once

namespace fuujin {
    // abstracts Renderer::PushRenderLabel and Renderer::PopRenderLabel
    class RenderLabel {
    public:
        RenderLabel();
        ~RenderLabel();

        RenderLabel(const std::string& label);

        RenderLabel(const RenderLabel&) = delete;
        RenderLabel& operator=(const RenderLabel&) = delete;

        RenderLabel(RenderLabel&& other);
        RenderLabel& operator=(RenderLabel&& other);

        const std::string& Get() const { return m_Label; }

    private:
        std::string m_Label;
        bool m_Pushed;
    };
} // namespace fuujin
