#include "fuujinpch.h"
#include "fuujin/platform/desktop/DesktopWindow.h"

#include "fuujin/Application.h"
#include "fuujin/Events.h"

#include <GLFW/glfw3.h>

namespace fuujin {
    static uint32_t s_WindowCount = 0;
    static std::mutex s_WindowMutex;

    static void FramebufferResized(GLFWwindow* window, int width, int height) {
        ViewSize size;
        size.Width = (uint32_t)std::max(0, width);
        size.Height = (uint32_t)std::max(0, height);

        FramebufferResizedEvent event(size);
        try {
            auto& app = Application::Get();
            app.ProcessEvent(event);
        } catch (const std::runtime_error&) {
            // nothing
        }
    }

    DesktopWindow::DesktopWindow(const std::string& title, const ViewSize& size) {
        ZoneScoped;

        std::lock_guard lock(s_WindowMutex);
        if (s_WindowCount++ == 0 && glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("Failed to initialize GLFW!");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_Window = glfwCreateWindow(size.Width, size.Height, title.c_str(), nullptr, nullptr);
        if (m_Window == nullptr) {
            throw std::runtime_error("Failed to create window!");
        }

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetFramebufferSizeCallback(m_Window, FramebufferResized);
    }

    DesktopWindow::~DesktopWindow() {
        ZoneScoped;

        std::lock_guard lock(s_WindowMutex);
        glfwDestroyWindow(m_Window);

        if (--s_WindowCount == 0) {
            glfwTerminate();
        }
    }

    void DesktopWindow::Update() {
        ZoneScoped;
        glfwPollEvents();
    }

    bool DesktopWindow::IsClosed() const {
        ZoneScoped;
        return glfwWindowShouldClose(m_Window) == GLFW_TRUE;
    }

    ViewSize&& DesktopWindow::GetSize() const {
        ZoneScoped;
        
        int width, height;
        glfwGetWindowSize(m_Window, &width, &height);

        ViewSize size;
        size.Width = (uint32_t)std::max(0, width);
        size.Height = (uint32_t)std::max(0, height);

        return std::move(size);
    }

    ViewSize&& DesktopWindow::GetFramebufferSize() const {
        ZoneScoped;

        int width, height;
        glfwGetFramebufferSize(m_Window, &width, &height);

        ViewSize size;
        size.Width = (uint32_t)std::max(0, width);
        size.Height = (uint32_t)std::max(0, height);

        return std::move(size);
    }

    void DesktopWindow::RequestSize(const ViewSize& size) {
        ZoneScoped;
        glfwSetWindowSize(m_Window, size.Width, size.Height);
    }
} // namespace fuujin