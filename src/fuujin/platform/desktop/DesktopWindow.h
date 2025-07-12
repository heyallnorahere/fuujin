#pragma once
#include "fuujin/core/View.h"

struct GLFWwindow;

namespace fuujin {
    class DesktopPlatform;
    class DesktopWindow : public View {
    public:
        DesktopWindow(const std::string& title, const ViewSize& size,
                      const DesktopPlatform* platform);

        virtual ~DesktopWindow() override;

        virtual void Update() override;
        virtual bool IsClosed() const override;

        virtual uint64_t GetID() const override { return m_ID; }

        virtual ViewSize GetSize() const override;
        virtual ViewSize GetFramebufferSize() const override;
        virtual void SetSize(const ViewSize& size) override;

        virtual void GetPosition(uint32_t& x, uint32_t& y) const override;
        virtual void SetPosition(uint32_t x, uint32_t y) override;

        virtual void SetCursor(Cursor cursor) override;

        virtual void GetCursorPosition(double& x, double& y) const override;
        virtual void SetCursorPosition(double x, double y) override;

        virtual void GetRequiredVulkanExtensions(
            std::vector<std::string>& extensions) const override;

        virtual void* CreateVulkanSurface(void* instance) const override;

    private:
        GLFWwindow* m_Window;
        uint64_t m_ID;

        const DesktopPlatform* m_Platform;
    };
} // namespace fuujin