#pragma once
#include "fuujin/core/View.h"

struct GLFWwindow;

namespace fuujin {
    class DesktopPlatform;
    class DesktopWindow : public View {
    public:
        DesktopWindow(const std::string& title, const ViewSize& size,
                      const ViewCreationOptions& options, const DesktopPlatform* platform);

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

        virtual bool IsFocused() const override;
        virtual bool IsMinimized() const override;
        virtual bool IsHovered() const override;

        virtual void SetTitle(const std::string& title) override;
        virtual void SetAlpha(float alpha) override;
        virtual void SetPassthrough(bool noInput) override;

        virtual void Focus() override;
        virtual void Show() override;

        virtual void GetRequiredVulkanExtensions(
            std::vector<std::string>& extensions) const override;

        virtual void* CreateVulkanSurface(void* instance) const override;

    private:
        GLFWwindow* m_Window;
        uint64_t m_ID;

        const DesktopPlatform* m_Platform;
    };
} // namespace fuujin