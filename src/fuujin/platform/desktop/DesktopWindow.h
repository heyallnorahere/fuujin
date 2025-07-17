#pragma once
#include "fuujin/core/View.h"

#include "fuujin/platform/desktop/DesktopPlatform.h"

struct GLFWwindow;

namespace fuujin {
    class DesktopWindow : public View {
    public:
        DesktopWindow(const std::string& title, const ViewSize& size,
                      const ViewCreationOptions& options, const Ref<DesktopPlatform>& platform);

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
        virtual bool IsCursorDisabled() const override;

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

        virtual std::string GetClipboardString() override;
        virtual void SetClipboardString(const std::string& value) override;

        void SendResizeEvent(const std::optional<ViewSize>& newSize,
                             const std::optional<ViewSize>& newFramebufferSize);

    private:
        GLFWwindow* m_Window;
        uint64_t m_ID;

        Ref<DesktopPlatform> m_Platform;
        std::optional<ViewSize> m_NewSize, m_NewFramebufferSize;
    };
} // namespace fuujin
