#include "fuujinpch.h"
#include "fuujin/platform/desktop/DesktopWindow.h"

#include "fuujin/core/Application.h"
#include "fuujin/core/Events.h"

#ifdef FUUJIN_PLATFORM_vulkan
#include "fuujin/platform/vulkan/VulkanContext.h"
#endif

#include "fuujin/platform/desktop/DesktopPlatform.h"

#include <GLFW/glfw3.h>

namespace fuujin {
    static uint64_t s_WindowID = 0;

    static void FramebufferResizedGLFW(GLFWwindow* window, int width, int height) {
        ZoneScoped;

        const char* windowTitle = glfwGetWindowTitle(window);
        FUUJIN_DEBUG("GLFW window {} resized to ({}, {})", windowTitle, width, height);

        ViewSize size;
        size.Width = (uint32_t)std::max(0, width);
        size.Height = (uint32_t)std::max(0, height);

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        FramebufferResizedEvent event(size, id);
        Application* app = nullptr;

        try {
            app = &Application::Get();
        } catch (const std::runtime_error&) {
            return;
        }

        app->ProcessEvent(event);
    }

    DesktopWindow::DesktopWindow(const std::string& title, const ViewSize& size,
                                 const DesktopPlatform* platform) {
        ZoneScoped;
        FUUJIN_DEBUG("Creating GLFW window: {} ({}, {})", title.c_str(), size.Width, size.Height);

        m_ID = s_WindowID++;
        m_Platform = platform;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_Window = glfwCreateWindow(size.Width, size.Height, title.c_str(), nullptr, nullptr);
        if (m_Window == nullptr) {
            throw std::runtime_error("Failed to create window!");
        }

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetFramebufferSizeCallback(m_Window, FramebufferResizedGLFW);
    }

    DesktopWindow::~DesktopWindow() {
        ZoneScoped;

        glfwDestroyWindow(m_Window);
    }

    void DesktopWindow::Update() {
        ZoneScoped;
        glfwPollEvents();
    }

    bool DesktopWindow::IsClosed() const {
        ZoneScoped;
        return glfwWindowShouldClose(m_Window) == GLFW_TRUE;
    }

    ViewSize DesktopWindow::GetSize() const {
        ZoneScoped;

        int width, height;
        glfwGetWindowSize(m_Window, &width, &height);

        ViewSize size;
        size.Width = (uint32_t)std::max(0, width);
        size.Height = (uint32_t)std::max(0, height);

        return std::move(size);
    }

    ViewSize DesktopWindow::GetFramebufferSize() const {
        ZoneScoped;

        int width, height;
        glfwGetFramebufferSize(m_Window, &width, &height);

        ViewSize size;
        size.Width = (uint32_t)std::max(0, width);
        size.Height = (uint32_t)std::max(0, height);

        return std::move(size);
    }

    void DesktopWindow::SetSize(const ViewSize& size) {
        ZoneScoped;
        glfwSetWindowSize(m_Window, (int)size.Width, (int)size.Height);
    }

    void DesktopWindow::GetPosition(uint32_t& x, uint32_t& y) const {
        ZoneScoped;

        int _x, _y;
        glfwGetWindowPos(m_Window, &_x, &_y);

        x = (uint32_t)_x;
        y = (uint32_t)_y;
    }

    void DesktopWindow::SetPosition(uint32_t x, uint32_t y) {
        ZoneScoped;
        glfwSetWindowPos(m_Window, (int)x, (int)y);
    }

    void DesktopWindow::SetCursor(Cursor cursor) {
        ZoneScoped;

        if (cursor != Cursor::Disabled) {
            auto glfwCursor = m_Platform->GetCursor(cursor);
            if (glfwCursor == nullptr) {
                FUUJIN_ERROR("Failed to retrieve GLFW cursor - skipping cursor set");
                return;
            }

            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glfwSetCursor(m_Window, glfwCursor);
        } else {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    void DesktopWindow::GetCursorPosition(double& x, double& y) const {
        ZoneScoped;
        glfwGetCursorPos(m_Window, &x, &y);
    }

    void DesktopWindow::SetCursorPosition(double x, double y) {
        ZoneScoped;
        glfwSetCursorPos(m_Window, x, y);
    }

    void DesktopWindow::GetRequiredVulkanExtensions(std::vector<std::string>& extensions) const {
        ZoneScoped;
        extensions.clear();

#ifdef FUUJIN_PLATFORM_vulkan
        uint32_t extensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

        for (uint32_t i = 0; i < extensionCount; i++) {
            extensions.push_back(glfwExtensions[i]);
        }
#endif
    }

    void* DesktopWindow::CreateVulkanSurface(void* instance) const {
        ZoneScoped;

#ifdef FUUJIN_PLATFORM_vulkan
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkResult result = glfwCreateWindowSurface((VkInstance)instance, m_Window,
                                                  &VulkanContext::GetAllocCallbacks(), &surface);

        if (result == VK_SUCCESS) {
            return surface;
        } else {
            return VK_NULL_HANDLE;
        }
#else
        return nullptr;
#endif
    }
} // namespace fuujin