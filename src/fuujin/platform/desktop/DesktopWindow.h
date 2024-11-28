#pragma once
#include "fuujin/core/View.h"

struct GLFWwindow;

namespace fuujin {
    class DesktopWindow : public View {
    public:
        DesktopWindow(const std::string& title, const ViewSize& size);
        virtual ~DesktopWindow() override;

        virtual void Update() override;
        virtual bool IsClosed() const override;

        virtual ViewSize GetSize() const override;
        virtual ViewSize GetFramebufferSize() const override;
        virtual void RequestSize(const ViewSize& size) override;

        virtual void GetRequiredVulkanExtensions(
            std::vector<std::string>& extensions) const override;
            
        virtual void* CreateVulkanSurface(void* instance) const override;

    private:
        GLFWwindow* m_Window;
    };
} // namespace fuujin