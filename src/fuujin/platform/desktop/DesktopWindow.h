#pragma once
#include "fuujin/View.h"

struct GLFWwindow;

namespace fuujin {
    class DesktopWindow : public View {
    public:
        DesktopWindow(const std::string& title, const ViewSize& size);
        virtual ~DesktopWindow() override;

        virtual void Update() override;
        virtual bool IsClosed() const override;

        virtual ViewSize&& GetSize() const override;
        virtual ViewSize&& GetFramebufferSize() const override;
        virtual void RequestSize(const ViewSize& size) override;

    private:
        GLFWwindow* m_Window;
    };
} // namespace fuujin