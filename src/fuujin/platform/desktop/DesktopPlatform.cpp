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

    Ref<View> DesktopPlatform::CreateView(const std::string& title, const ViewSize& size) const {
        ZoneScoped;

        return Ref<DesktopWindow>::Create(title, size, this);
    }

    GLFWcursor* DesktopPlatform::GetCursor(Cursor name) const {
        ZoneScoped;

        if (m_Cursors.contains(name)) {
            return m_Cursors.at(name);
        }

        return nullptr;
    }
} // namespace fuujin