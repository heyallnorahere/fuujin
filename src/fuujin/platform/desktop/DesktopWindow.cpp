#include "fuujinpch.h"

#ifdef FUUJIN_PLATFORM_vulkan
#include "fuujin/platform/vulkan/VulkanContext.h"
#endif

#include "fuujin/platform/desktop/DesktopWindow.h"

#include "fuujin/core/Application.h"
#include "fuujin/core/Events.h"

// from imgui_impl_glfw
static int ImGui_ImplGlfw_TranslateUntranslatedKey(int key, int scancode) {
#if GLFW_HAS_GETKEYNAME && !defined(EMSCRIPTEN_USE_EMBEDDED_GLFW3)
    // GLFW 3.1+ attempts to "untranslate" keys, which goes the opposite of what every other
    // framework does, making using lettered shortcuts difficult. (It had reasons to do so: namely
    // GLFW is/was more likely to be used for WASD-type game controls rather than lettered
    // shortcuts, but IHMO the 3.1 change could have been done differently) See
    // https://github.com/glfw/glfw/issues/1502 for details. Adding a workaround to undo this (so
    // our keys are translated->untranslated->translated, likely a lossy process). This won't cover
    // edge cases but this is at least going to cover common cases.
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL)
        return key;
    GLFWerrorfun prev_error_callback = glfwSetErrorCallback(nullptr);
    const char* key_name = glfwGetKeyName(key, scancode);
    glfwSetErrorCallback(prev_error_callback);
#if GLFW_HAS_GETERROR && !defined(EMSCRIPTEN_USE_EMBEDDED_GLFW3) // Eat errors (see #5908)
    (void)glfwGetError(nullptr);
#endif
    if (key_name && key_name[0] != 0 && key_name[1] == 0) {
        const char char_names[] = "`-=[]\\,;\'./";
        const int char_keys[] = {
            GLFW_KEY_GRAVE_ACCENT,  GLFW_KEY_MINUS,     GLFW_KEY_EQUAL, GLFW_KEY_LEFT_BRACKET,
            GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_BACKSLASH, GLFW_KEY_COMMA, GLFW_KEY_SEMICOLON,
            GLFW_KEY_APOSTROPHE,    GLFW_KEY_PERIOD,    GLFW_KEY_SLASH, 0
        };
        // IM_ASSERT(IM_ARRAYSIZE(char_names) == IM_ARRAYSIZE(char_keys));
        if (key_name[0] >= '0' && key_name[0] <= '9') {
            key = GLFW_KEY_0 + (key_name[0] - '0');
        } else if (key_name[0] >= 'A' && key_name[0] <= 'Z') {
            key = GLFW_KEY_A + (key_name[0] - 'A');
        } else if (key_name[0] >= 'a' && key_name[0] <= 'z') {
            key = GLFW_KEY_A + (key_name[0] - 'a');
        } else if (const char* p = strchr(char_names, key_name[0])) {
            key = char_keys[p - char_names];
        }
    }
    // if (action == GLFW_PRESS) printf("key %d scancode %d name '%s'\n", key, scancode, key_name);
#else
    // IM_UNUSED(scancode);
#endif
    return key;
}

namespace fuujin {
    static uint64_t s_WindowID = 0;

    static void GLFW_WindowSize(GLFWwindow* window, int width, int height) {
        ZoneScoped;

        ViewSize size;
        size.Width = (uint32_t)width;
        size.Height = (uint32_t)height;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        view->SendResizeEvent(size, {});
    }

    static void GLFW_WindowPos(GLFWwindow* window, int x, int y) {
        ZoneScoped;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        ViewMovedEvent event((int32_t)x, (int32_t)y, id);
        Application::ProcessEvent(event);
    }

    static void GLFW_FramebufferSize(GLFWwindow* window, int width, int height) {
        ZoneScoped;

        ViewSize framebufferSize;
        framebufferSize.Width = (uint32_t)width;
        framebufferSize.Height = (uint32_t)height;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        view->SendResizeEvent({}, framebufferSize);
    }

    static void GLFW_WindowFocus(GLFWwindow* window, int focused) {
        ZoneScoped;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        ViewFocusedEvent event(focused != GLFW_FALSE, id);
        Application::ProcessEvent(event);
    }

    static void GLFW_CursorEnter(GLFWwindow* window, int entered) {
        ZoneScoped;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        CursorEnteredEvent event(entered != GLFW_FALSE, id);
        Application::ProcessEvent(event);
    }

    static void GLFW_CursorPos(GLFWwindow* window, double x, double y) {
        ZoneScoped;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        CursorPositionEvent event(x, y, id);
        Application::ProcessEvent(event);
    }

    static void GLFW_MouseButton(GLFWwindow* window, int button, int action, int mods) {
        ZoneScoped;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        MouseButtonEvent event(button, action == GLFW_PRESS, id);
        Application::ProcessEvent(event);
    }

    static void GLFW_Scroll(GLFWwindow* window, double xOffset, double yOffset) {
        ZoneScoped;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        ScrollEvent event(xOffset, yOffset, id);
        Application::ProcessEvent(event);
    }

    static Key TranslateGLFWKey(int glfwKey) {
#define TRANSLATE(GLFW, FUUJIN)                                                                    \
    case GLFW_KEY_##GLFW:                                                                          \
        return Key::FUUJIN

        switch (glfwKey) {
            TRANSLATE(TAB, Tab);
            TRANSLATE(LEFT, Left);
            TRANSLATE(RIGHT, Right);
            TRANSLATE(UP, Up);
            TRANSLATE(DOWN, Down);
            TRANSLATE(PAGE_UP, PageUp);
            TRANSLATE(PAGE_DOWN, PageDown);
            TRANSLATE(HOME, Home);
            TRANSLATE(END, End);
            TRANSLATE(INSERT, Insert);
            TRANSLATE(DELETE, Delete);
            TRANSLATE(BACKSPACE, Backspace);
            TRANSLATE(SPACE, Space);
            TRANSLATE(ENTER, Enter);
            TRANSLATE(ESCAPE, Escape);
            TRANSLATE(APOSTROPHE, Apostrophe);
            TRANSLATE(COMMA, Comma);
            TRANSLATE(MINUS, Minus);
            TRANSLATE(PERIOD, Period);
            TRANSLATE(SLASH, Slash);
            TRANSLATE(SEMICOLON, Semicolon);
            TRANSLATE(EQUAL, Equal);
            TRANSLATE(LEFT_BRACKET, LeftBracket);
            TRANSLATE(BACKSLASH, Backslash);
            TRANSLATE(WORLD_1, Oem102);
            TRANSLATE(WORLD_2, Oem102);
            TRANSLATE(RIGHT_BRACKET, RightBracket);
            TRANSLATE(GRAVE_ACCENT, GraveAccent);
            TRANSLATE(CAPS_LOCK, CapsLock);
            TRANSLATE(SCROLL_LOCK, ScrollLock);
            TRANSLATE(NUM_LOCK, NumLock);
            TRANSLATE(PRINT_SCREEN, PrintScreen);
            TRANSLATE(PAUSE, Pause);
            TRANSLATE(KP_0, Keypad0);
            TRANSLATE(KP_1, Keypad1);
            TRANSLATE(KP_2, Keypad2);
            TRANSLATE(KP_3, Keypad3);
            TRANSLATE(KP_4, Keypad4);
            TRANSLATE(KP_5, Keypad5);
            TRANSLATE(KP_6, Keypad6);
            TRANSLATE(KP_7, Keypad7);
            TRANSLATE(KP_8, Keypad8);
            TRANSLATE(KP_9, Keypad9);
            TRANSLATE(KP_DECIMAL, KeypadDecimal);
            TRANSLATE(KP_DIVIDE, KeypadDivide);
            TRANSLATE(KP_MULTIPLY, KeypadMultiply);
            TRANSLATE(KP_SUBTRACT, KeypadSubtract);
            TRANSLATE(KP_ADD, KeypadAdd);
            TRANSLATE(KP_ENTER, KeypadEnter);
            TRANSLATE(KP_EQUAL, KeypadEqual);
            TRANSLATE(LEFT_SHIFT, LeftShift);
            TRANSLATE(LEFT_CONTROL, LeftControl);
            TRANSLATE(LEFT_ALT, LeftAlt);
            TRANSLATE(LEFT_SUPER, LeftSuper);
            TRANSLATE(RIGHT_SHIFT, RightShift);
            TRANSLATE(RIGHT_CONTROL, RightControl);
            TRANSLATE(RIGHT_ALT, RightAlt);
            TRANSLATE(RIGHT_SUPER, RightSuper);
            TRANSLATE(MENU, Menu);
            TRANSLATE(0, _0);
            TRANSLATE(1, _1);
            TRANSLATE(2, _2);
            TRANSLATE(3, _3);
            TRANSLATE(4, _4);
            TRANSLATE(5, _5);
            TRANSLATE(6, _6);
            TRANSLATE(7, _7);
            TRANSLATE(8, _8);
            TRANSLATE(9, _9);
            TRANSLATE(A, A);
            TRANSLATE(B, B);
            TRANSLATE(C, C);
            TRANSLATE(D, D);
            TRANSLATE(E, E);
            TRANSLATE(F, F);
            TRANSLATE(G, G);
            TRANSLATE(H, H);
            TRANSLATE(I, I);
            TRANSLATE(J, J);
            TRANSLATE(K, K);
            TRANSLATE(L, L);
            TRANSLATE(M, M);
            TRANSLATE(N, N);
            TRANSLATE(O, O);
            TRANSLATE(P, P);
            TRANSLATE(Q, Q);
            TRANSLATE(R, R);
            TRANSLATE(S, S);
            TRANSLATE(T, T);
            TRANSLATE(U, U);
            TRANSLATE(V, V);
            TRANSLATE(W, W);
            TRANSLATE(X, X);
            TRANSLATE(Y, Y);
            TRANSLATE(Z, Z);
            TRANSLATE(F1, F1);
            TRANSLATE(F2, F2);
            TRANSLATE(F3, F3);
            TRANSLATE(F4, F4);
            TRANSLATE(F5, F5);
            TRANSLATE(F6, F6);
            TRANSLATE(F7, F7);
            TRANSLATE(F8, F8);
            TRANSLATE(F9, F9);
            TRANSLATE(F10, F10);
            TRANSLATE(F11, F11);
            TRANSLATE(F12, F12);
            TRANSLATE(F13, F13);
            TRANSLATE(F14, F14);
            TRANSLATE(F15, F15);
            TRANSLATE(F16, F16);
            TRANSLATE(F17, F17);
            TRANSLATE(F18, F18);
            TRANSLATE(F19, F19);
            TRANSLATE(F20, F20);
            TRANSLATE(F21, F21);
            TRANSLATE(F22, F22);
            TRANSLATE(F23, F23);
            TRANSLATE(F24, F24);
        default:
            return Key::None;
        }
    }

    static void GLFW_Key(GLFWwindow* window, int keycode, int scancode, int action, int mods) {
        ZoneScoped;

        if (action != GLFW_PRESS && action != GLFW_RELEASE) {
            return;
        }

        int glfwKey = ImGui_ImplGlfw_TranslateUntranslatedKey(keycode, scancode);
        auto key = TranslateGLFWKey(glfwKey);

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        KeyEvent event(key, action == GLFW_PRESS, id);
        Application::ProcessEvent(event);
    }

    static void GLFW_Char(GLFWwindow* window, unsigned int character) {
        ZoneScoped;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        CharEvent event(character, id);
        Application::ProcessEvent(event);
    }

    static void GLFW_WindowClose(GLFWwindow* window) {
        ZoneScoped;

        auto view = (DesktopWindow*)glfwGetWindowUserPointer(window);
        uint64_t id = view->GetID();

        ViewClosedEvent event(id);
        Application::ProcessEvent(event);
    }

    DesktopWindow::DesktopWindow(const std::string& title, const ViewSize& size,
                                 const ViewCreationOptions& options,
                                 const Ref<DesktopPlatform>& platform) {
        ZoneScoped;
        FUUJIN_DEBUG("Creating GLFW window: {} ({}, {})", title.c_str(), size.Width, size.Height);

        m_ID = s_WindowID++;
        m_Platform = platform;

        glfwWindowHint(GLFW_VISIBLE, options.Visible);
        glfwWindowHint(GLFW_FOCUSED, options.Focused);
#if GLFW_HAS_FOCUS_ON_SHOW
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, options.FocusOnShow);
#endif
        glfwWindowHint(GLFW_DECORATED, options.Decorated);
#if GLFW_HAS_WINDOW_TOPMOST
        glfwWindowHint(GLFW_FLOATING, options.Floating);
#endif

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_Window = glfwCreateWindow(size.Width, size.Height, title.c_str(), nullptr, nullptr);
        if (m_Window == nullptr) {
            throw std::runtime_error("Failed to create window!");
        }

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetWindowSizeCallback(m_Window, GLFW_WindowSize);
        glfwSetWindowPosCallback(m_Window, GLFW_WindowPos);
        glfwSetFramebufferSizeCallback(m_Window, GLFW_FramebufferSize);
        glfwSetWindowFocusCallback(m_Window, GLFW_WindowFocus);
        glfwSetCursorEnterCallback(m_Window, GLFW_CursorEnter);
        glfwSetCursorPosCallback(m_Window, GLFW_CursorPos);
        glfwSetMouseButtonCallback(m_Window, GLFW_MouseButton);
        glfwSetScrollCallback(m_Window, GLFW_Scroll);
        glfwSetKeyCallback(m_Window, GLFW_Key);
        glfwSetCharCallback(m_Window, GLFW_Char);
        glfwSetWindowCloseCallback(m_Window, GLFW_WindowClose);
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

#if defined(__APPLE__) && !GLFW_HAS_OSX_WINDOW_POS_FIX
        int x, y, width, height;
        glfwGetWindowPos(m_Window, &x, &y);
        glfwGetWindowSize(m_Window, &width, &height);
        glfwSetWindowPos(m_Window, x, y - height + (int)size.Height);
#endif

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

        switch (cursor) {
        case Cursor::Disabled:
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
        case Cursor::None:
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        default: {
            auto glfwCursor = m_Platform->GetCursor(cursor);
            if (glfwCursor == nullptr) {
                FUUJIN_ERROR("Failed to retrieve GLFW cursor - skipping cursor set");
                return;
            }

            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glfwSetCursor(m_Window, glfwCursor);
        } break;
        }
    }

    bool DesktopWindow::IsCursorDisabled() const {
        ZoneScoped;

        int cursorMode = glfwGetInputMode(m_Window, GLFW_CURSOR);
        return cursorMode == GLFW_CURSOR_DISABLED;
    }

    void DesktopWindow::GetCursorPosition(double& x, double& y) const {
        ZoneScoped;
        glfwGetCursorPos(m_Window, &x, &y);
    }

    void DesktopWindow::SetCursorPosition(double x, double y) {
        ZoneScoped;
        glfwSetCursorPos(m_Window, x, y);
    }

    bool DesktopWindow::IsFocused() const {
        ZoneScoped;
#ifdef EMSCRIPTEN_USE_EMBEDDED_GLFW3
        return true;
#else
        return glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) != GLFW_FALSE;
#endif
    }

    bool DesktopWindow::IsMinimized() const {
        ZoneScoped;
        return glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED) != GLFW_FALSE;
    }

    bool DesktopWindow::IsHovered() const {
        ZoneScoped;

#if GLFW_HAS_MOUSE_PASSTHROUGH || GLFW_HAS_WINDOW_HOVERED
        return glfwGetWindowAttrib(m_Window, GLFW_HOVERED) != GLFW_FALSE;
#else
        return false;
#endif
    }

    void DesktopWindow::SetTitle(const std::string& title) {
        ZoneScoped;
        glfwSetWindowTitle(m_Window, title.c_str());
    }

    void DesktopWindow::SetAlpha(float alpha) {
        ZoneScoped;
        glfwSetWindowOpacity(m_Window, alpha);
    }

    void DesktopWindow::SetPassthrough(bool noInput) {
        ZoneScoped;
#if GLFW_HAS_PASSTHROUGH
        glfwSetWindowAttrib(m_Window, GLFW_MOUSE_PASSTHROUGH, noInput);
#endif
    }

    void DesktopWindow::Focus() {
        ZoneScoped;
        glfwFocusWindow(m_Window);
    }

    void DesktopWindow::Show() {
        ZoneScoped;

        // imgui glfw backend does some win32 shenanigans here to differentiate between
        // mouse/touchscreen/pen. we dont need that (for now)

        glfwShowWindow(m_Window);
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

    void DesktopWindow::SendResizeEvent(const std::optional<ViewSize>& newSize,
                                        const std::optional<ViewSize>& newFramebufferSize) {
        ZoneScoped;

        if (newSize.has_value()) {
            m_NewSize = newSize;
        }

        if (newFramebufferSize.has_value()) {
            m_NewFramebufferSize = newFramebufferSize;
        }

        if (m_NewSize.has_value() && m_NewFramebufferSize.has_value()) {
            ViewResizedEvent event(m_NewSize.value(), m_NewFramebufferSize.value(), m_ID);
            Application::ProcessEvent(event);

            m_NewSize.reset();
            m_NewFramebufferSize.reset();
        }
    }
} // namespace fuujin