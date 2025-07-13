#pragma once
#include "fuujin/core/Platform.h"

#include <GLFW/glfw3.h>

// lifted from imgui_impl_glfw.cpp
#define GLFW_VERSION (GLFW_VERSION_MAJOR * 1000 + GLFW_VERSION_MINOR * 100 + GLFW_VERSION_REVISION)
#define GLFW_HAS_WINDOW_TOPMOST (GLFW_VERSION >= 3200)
#define GLFW_HAS_FOCUS_WINDOW (GLFW_VERSION >= 3200)
#define GLFW_HAS_GETKEYNAME (GLFW_VERSION >= 3200)
#define GLFW_HAS_WINDOW_HOVERED (GLFW_VERSION >= 3300)
#define GLFW_HAS_WINDOW_ALPHA (GLFW_VERSION >= 3300)
#define GLFW_HAS_PER_MONITOR_DPI (GLFW_VERSION >= 3300)
#define GLFW_HAS_FOCUS_ON_SHOW (GLFW_VERSION >= 3300)
#define GLFW_HAS_MONITOR_WORK_AREA (GLFW_VERSION >= 3300)
#define GLFW_HAS_GAMEPAD_API (GLFW_VERSION >= 3300)
#define GLFW_HAS_OSX_WINDOW_POS_FIX (GLFW_VERSION >= 3301)
#define GLFW_HAS_GETPLATFORM (GLFW_VERSION >= 3400)

#ifdef GLFW_RESIZE_NESW_CURSOR
#define GLFW_HAS_NEW_CURSORS (GLFW_VERSION >= 3400)
#endif

#ifdef GLFW_MOUSE_PASSTHROUGH
#define GLFW_HAS_MOUSE_PASSTHROUGH (GLFW_VERSION >= 3400)
#endif

namespace fuujin {
    class DesktopPlatform : public PlatformAPI {
    public:
        DesktopPlatform();
        virtual ~DesktopPlatform() override;

        virtual bool HasCursors() const override { return true; }
        virtual bool CanSetCursorPos() const override { return true; }

        virtual bool HasViewports() const override;
        virtual bool HasViewHovered() const override;

        virtual std::string GetName() const override;

        virtual Ref<View> CreateView(const std::string& title, const ViewSize& size,
                                     const ViewCreationOptions& options) const override;

        virtual bool QueryMonitors(std::vector<MonitorInfo>& monitors) const override;

        GLFWcursor* GetCursor(Cursor name) const;

    private:
        std::map<Cursor, GLFWcursor*> m_Cursors;
    };
} // namespace fuujin