#include "fuujinpch.h"
#include "fuujin/platform/desktop/DesktopWindow.h"

#include "fuujin/core/Application.h"
#include "fuujin/core/Events.h"

#ifdef FUUJIN_PLATFORM_vulkan
#include "fuujin/platform/vulkan/VulkanContext.h"
#endif

#include <GLFW/glfw3.h>

namespace fuujin {
    static uint32_t s_WindowCount = 0;
    static std::mutex s_WindowMutex;

    static void FramebufferResizedGLFW(GLFWwindow* window, int width, int height) {
        ZoneScoped;

        const char* windowTitle = glfwGetWindowTitle(window);
        FUUJIN_DEBUG("GLFW window {} resized to ({}, {})", windowTitle, width, height);

        ViewSize size;
        size.Width = (uint32_t)std::max(0, width);
        size.Height = (uint32_t)std::max(0, height);

        FramebufferResizedEvent event(size);
        Application* app = nullptr;

        try {
            app = &Application::Get();
        } catch (const std::runtime_error&) {
            return;
        }

        app->ProcessEvent(event);
    }

    DesktopWindow::DesktopWindow(const std::string& title, const ViewSize& size) {
        ZoneScoped;
        FUUJIN_DEBUG("Creating GLFW window: {} ({}, {})", title.c_str(), size.Width, size.Height);

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
        glfwSetFramebufferSizeCallback(m_Window, FramebufferResizedGLFW);
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

    void DesktopWindow::RequestSize(const ViewSize& size) {
        ZoneScoped;
        glfwSetWindowSize(m_Window, size.Width, size.Height);
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