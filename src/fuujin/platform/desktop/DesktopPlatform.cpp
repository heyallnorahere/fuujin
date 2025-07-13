#include "fuujinpch.h"
#include "fuujin/platform/desktop/DesktopPlatform.h"

#include "fuujin/platform/desktop/DesktopWindow.h"

#include <GLFW/glfw3.h>

namespace fuujin {
    bool s_GLFWInitialized = false;

    DesktopPlatform::DesktopPlatform() {
        ZoneScoped;

        if (s_GLFWInitialized) {
            throw std::runtime_error("DesktopPlatform already initialized!");
        }

        if (glfwInit() != GLFW_TRUE) {
            const char* description;
            int error = glfwGetError(&description);

            FUUJIN_ERROR("Failed to initialize GLFW: {} (error {})", description, error);
            return;
        }

        static const std::map<Cursor, int> standardCursors = {
            { Cursor::Arrow, GLFW_ARROW_CURSOR },
            { Cursor::TextInput, GLFW_IBEAM_CURSOR },
            { Cursor::Hand, GLFW_HAND_CURSOR },
            { Cursor::NotAllowed, GLFW_NOT_ALLOWED_CURSOR },
            { Cursor::ResizeNS, GLFW_VRESIZE_CURSOR },
            { Cursor::ResizeEW, GLFW_HRESIZE_CURSOR },
            { Cursor::ResizeNESW, GLFW_RESIZE_NESW_CURSOR },
            { Cursor::ResizeNWSE, GLFW_RESIZE_NWSE_CURSOR },
            { Cursor::ResizeAll, GLFW_RESIZE_ALL_CURSOR },
        };

        for (const auto& [name, shape] : standardCursors) {
            m_Cursors[name] = glfwCreateStandardCursor(shape);
        }

        s_GLFWInitialized = true;
    }

    DesktopPlatform::~DesktopPlatform() {
        ZoneScoped;

        for (const auto& [name, cursor] : m_Cursors) {
            glfwDestroyCursor(cursor);
        }

        glfwTerminate();
        s_GLFWInitialized = true;
    }

    bool DesktopPlatform::HasViewports() const {
        ZoneScoped;

#if GLFW_HAS_GETPLATFORM
        if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
            return false;
        }
#endif

        return true;
    }

    bool DesktopPlatform::HasViewHovered() const {
        ZoneScoped;

#if GLFW_HAS_MOUSE_PASSTHROUGH || GLFW_HAS_WINDOW_HOVERED
        return true;
#else
        return false;
#endif
    }

    std::string DesktopPlatform::GetName() const {
        ZoneScoped;

        std::string platformName;
#ifdef FUUJIN_PLATFORM_windows
        platformName = "Windows";
#elif defined(FUUJIN_PLATFORM_linux)
        platformName = "Linux";
#elif defined(FUUJIN_PLATFORM_osx)
        platformName = "MacOS";
#else
        platformName = "Unknown platform";
#endif

        return platformName + " via GLFW";
    }

    Ref<View> DesktopPlatform::CreateView(const std::string& title, const ViewSize& size,
                                          const ViewCreationOptions& options) const {
        ZoneScoped;

        return Ref<DesktopWindow>::Create(title, size, options, this);
    }

    static float GetMonitorContentScale(GLFWmonitor* monitor) {
        ZoneScoped;

#if GLFW_HAS_PER_MONITOR_DPI &&                                                                    \
    !(defined(__APPLE__) || defined(__EMSCRIPTEN__) || defined(__ANDROID__))
        float x, y;
        glfwGetMonitorContentScale(monitor, &x, &y);
        return x;
#else
        return 1.f;
#endif
    }

    bool DesktopPlatform::QueryMonitors(std::vector<MonitorInfo>& monitors) const {
        ZoneScoped;

        int count = 0;
        auto glfwMonitors = glfwGetMonitors(&count);

        if (count <= 0) {
            return false;
        }

        monitors.clear();
        for (int i = 0; i < count; i++) {
            auto monitor = glfwMonitors[i];

            int x, y;
            glfwGetMonitorPos(monitor, &x, &y);

            auto videoMode = glfwGetVideoMode(monitor);
            if (videoMode == nullptr) {
                continue;
            }

            auto& info = monitors.emplace_back();
            info.MainPos = glm::ivec2(x, y);
            info.MainSize = glm::ivec2(videoMode->width, videoMode->height);

            int width, height;
            glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);

            info.WorkPos = glm::ivec2(x, y);
            info.WorkSize = glm::ivec2(width, height);

            info.Scale = GetMonitorContentScale(monitor);
            info.Handle = monitor;
        }

        return true;
    }

    GLFWcursor* DesktopPlatform::GetCursor(Cursor name) const {
        ZoneScoped;

        if (m_Cursors.contains(name)) {
            return m_Cursors.at(name);
        }

        return nullptr;
    }
} // namespace fuujin