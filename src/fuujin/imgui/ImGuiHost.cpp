#include "fuujinpch.h"
#include "fuujin/imgui/ImGuiHost.h"

#include "fuujin/renderer/DeviceBuffer.h"
#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/ShaderLibrary.h"
#include "fuujin/renderer/ShaderBuffer.h"

#include "fuujin/core/Application.h"
#include "fuujin/core/Events.h"
#include "fuujin/core/Platform.h"

// combination of imgui_impl_glfw and imgui_impl_vulkan using the abstracted interfaces

namespace fuujin {
    struct ImGuiVertex {
        glm::vec2 Position;
        glm::vec2 UV;
        glm::vec4 Color;
    };

    struct ViewportPlatformData {
        Ref<View> ViewportView;
        int IgnoreResizeFrame, IgnoreMoveFrame;
    };

    struct ImGuiRenderBuffers {
        Ref<DeviceBuffer> VertexBuffer, IndexBuffer;
    };

    struct ViewportRendererData {
        Ref<Swapchain> ViewSwapchain;
        Ref<Pipeline> ViewPipeline;

        std::vector<ImGuiRenderBuffers> RenderBuffers;
    };

    struct ImGuiHostData {
        ImGuiContext* Context;
        std::string PlatformName, RendererName;

        Ref<View> MainView;
        std::string Clipboard;
        std::optional<std::chrono::high_resolution_clock::time_point> Time;

        bool IgnoreMouseUp;
        bool IgnoreMouseUpOnFocusLoss;

        std::optional<uint64_t> MouseView;
        ImVec2 LastValidMousePos;

        Ref<GraphicsContext> RendererContext;
        Ref<Shader> ImGuiShader;

        uint32_t MainRenderTarget;
        std::map<uint32_t, Ref<Pipeline>> MainViewportPipelines;

        ImTextureID TextureID;
        std::map<ImTextureID, Ref<RendererAllocation>> TextureAllocations;
        std::map<uint64_t, ImTextureID> TextureMap;
        std::unordered_map<int, Ref<Texture>> ImGuiTextures;
    };

    static std::unique_ptr<ImGuiHostData> s_Data;

    static void* ImGui_Alloc(size_t size, void* userData) { return allocate(size); }
    static void ImGui_Free(void* block, void* userData) { freemem(block); }

    void ImGuiHost::Init() {
        ZoneScoped;

        if (s_Data) {
            return;
        }

        s_Data = std::make_unique<ImGuiHostData>();
        ImGui::SetAllocatorFunctions(ImGui_Alloc, ImGui_Free);

        IMGUI_CHECKVERSION();
        s_Data->Context = ImGui::CreateContext();
        ImGui::SetCurrentContext(s_Data->Context);

        auto& io = ImGui::GetIO();
        (void)io; // verify that io is not null

        ImGui::StyleColorsDark();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGuiStyle& style = ImGui::GetStyle();
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
            style.WindowRounding = 0.f;
            style.Colors[ImGuiCol_WindowBg].w = 1.f;
        }

        Platform_Init();
        Renderer_Init();

        io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/roboto/Roboto-Medium.ttf");
    }

    void ImGuiHost::Shutdown() {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        ImGui::SetCurrentContext(s_Data->Context);

        Renderer_Shutdown();
        Platform_Shutdown();

        ImGui::SetCurrentContext(nullptr);
        ImGui::DestroyContext(s_Data->Context);

        s_Data.reset();
    }

    static bool ImGui_OnResize(const ViewResizedEvent& event) {
        ZoneScoped;

        auto viewport = ImGui::FindViewportByPlatformHandle((void*)event.GetView());
        auto mainViewport = ImGui::GetMainViewport();

        // window is not ours or is main viewport (dont want to fuck with application)
        if (viewport == nullptr || viewport->ID == mainViewport->ID) {
            return false;
        }

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        auto rendererData = (ViewportRendererData*)viewport->RendererUserData;

        int currentFrame = ImGui::GetFrameCount();
        bool ignorePlatformEvent = currentFrame <= platformData->IgnoreResizeFrame + 1;
        if (!ignorePlatformEvent) {
            viewport->PlatformRequestResize = true;
        }

        if (rendererData != nullptr) {
            rendererData->ViewSwapchain->RequestResize(event.GetFramebufferSize());
        }

        return true;
    }

    static bool ImGui_OnMove(const ViewMovedEvent& event) {
        ZoneScoped;

        auto viewport = ImGui::FindViewportByPlatformHandle((void*)event.GetView());
        auto mainViewport = ImGui::GetMainViewport();

        // window is not ours or is main viewport (dont want to fuck with application)
        if (viewport == nullptr || viewport->ID == mainViewport->ID) {
            return false;
        }

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        int currentFrame = ImGui::GetFrameCount();
        bool ignore = currentFrame <= platformData->IgnoreMoveFrame + 1;

        if (!ignore) {
            viewport->PlatformRequestMove = true;
        }

        return true;
    }

    static bool ImGui_OnFocus(const ViewFocusedEvent& event) {
        ZoneScoped;

        auto viewport = ImGui::FindViewportByPlatformHandle((void*)event.GetView());
        if (viewport == nullptr) {
            return false;
        }

        auto mainViewport = ImGui::GetMainViewport();
        bool isMainViewport = viewport->ID == mainViewport->ID;

        bool focused = event.IsFocused();
        s_Data->IgnoreMouseUp = s_Data->IgnoreMouseUpOnFocusLoss && !focused;
        s_Data->IgnoreMouseUpOnFocusLoss = false;

        auto& io = ImGui::GetIO();
        io.AddFocusEvent(focused);

        return isMainViewport;
    }

    static bool ImGui_OnCursorEnter(const CursorEnteredEvent& event) {
        ZoneScoped;

        uint64_t id = event.GetView();
        auto viewport = ImGui::FindViewportByPlatformHandle((void*)id);

        if (viewport == nullptr) {
            return false; // we dont care
        }

        auto& io = ImGui::GetIO();
        if (event.DidEnter()) {
            s_Data->MouseView = id;

            io.AddMousePosEvent(s_Data->LastValidMousePos.x, s_Data->LastValidMousePos.y);
        } else {
            s_Data->LastValidMousePos = io.MousePos;
            s_Data->MouseView.reset();

            static constexpr float max = std::numeric_limits<float>::max();
            io.AddMousePosEvent(-max, -max);
        }

        auto mainViewport = ImGui::GetMainViewport();
        return viewport->ID != mainViewport->ID;
    }

    static bool ImGui_CursorPos(const CursorPositionEvent& event) {
        ZoneScoped;

        uint64_t id = event.GetView();
        auto mainViewport = ImGui::GetMainViewport();
        auto viewport = ImGui::FindViewportByPlatformHandle((void*)id);
        bool isMainViewport = viewport != nullptr && viewport->ID == mainViewport->ID;

        float x = (float)event.GetX();
        float y = (float)event.GetY();

        auto& io = ImGui::GetIO();
        bool viewportsEnabled = (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;

        if (viewportsEnabled || !isMainViewport) {
            auto view = Platform::GetViewByID(event.GetView());

            uint32_t windowX, windowY;
            view->GetPosition(windowX, windowY);

            x += windowX;
            y += windowY;

            // if viewports arent enabled, all mouse inputs are relative to the main viewport
            if (!viewportsEnabled && !isMainViewport) {
                auto platformData = (ViewportPlatformData*)mainViewport->PlatformUserData;
                auto mainView = platformData->ViewportView;

                mainView->GetPosition(windowX, windowY);

                x -= windowY;
                y -= windowY;
            }
        }

        io.AddMousePosEvent(x, y);
        s_Data->LastValidMousePos = ImVec2(x, y);

        // if imgui tells us to, we want to block main engine from receiving mouse inputs
        return io.WantCaptureMouse;
    }

    static bool ImGui_OnMouseButton(const MouseButtonEvent& event) {
        ZoneScoped;

        bool pressed = event.IsPressed();
        uint32_t button = event.GetButton();

        if (s_Data->IgnoreMouseUp && !pressed) {
            return false; // explicit ignore
        }

        uint64_t id = event.GetView();
        auto viewport = ImGui::FindViewportByPlatformHandle((void*)id);

        if (viewport == nullptr) {
            return false; // we dont care
        }

        auto& io = ImGui::GetIO();
        if (button < ImGuiMouseButton_COUNT) {
            io.AddMouseButtonEvent((int)button, pressed);
        }

        return io.WantCaptureMouse;
    }

    static bool ImGui_OnScroll(const ScrollEvent& event) {
        ZoneScoped;

        uint64_t id = event.GetView();
        auto viewport = ImGui::FindViewportByPlatformHandle((void*)id);

        if (viewport == nullptr) {
            return false; // we dont care
        }

        float xOffset = (float)event.GetXOffset();
        float yOffset = (float)event.GetYOffset();

        ImGuiIO& io = ImGui::GetIO();
        io.AddMouseWheelEvent(xOffset, yOffset);

        return io.WantCaptureMouse;
    }

    static ImGuiKey ToImGuiKey(Key key) {
#define TRANSLATE(fuujin, imgui)                                                                   \
    case Key::fuujin:                                                                              \
        return ImGuiKey_##imgui

        switch (key) {
            TRANSLATE(Tab, Tab);
            TRANSLATE(Left, LeftArrow);
            TRANSLATE(Right, RightArrow);
            TRANSLATE(Up, UpArrow);
            TRANSLATE(Down, DownArrow);
            TRANSLATE(PageUp, PageUp);
            TRANSLATE(PageDown, PageDown);
            TRANSLATE(Home, Home);
            TRANSLATE(End, End);
            TRANSLATE(Insert, Insert);
            TRANSLATE(Delete, Delete);
            TRANSLATE(Backspace, Backspace);
            TRANSLATE(Space, Space);
            TRANSLATE(Enter, Enter);
            TRANSLATE(Escape, Escape);
            TRANSLATE(Apostrophe, Apostrophe);
            TRANSLATE(Comma, Comma);
            TRANSLATE(Minus, Minus);
            TRANSLATE(Period, Period);
            TRANSLATE(Slash, Slash);
            TRANSLATE(Semicolon, Semicolon);
            TRANSLATE(Equal, Equal);
            TRANSLATE(LeftBracket, LeftBracket);
            TRANSLATE(Backslash, Backslash);
            TRANSLATE(Oem102, Oem102);
            TRANSLATE(RightBracket, RightBracket);
            TRANSLATE(GraveAccent, GraveAccent);
            TRANSLATE(CapsLock, CapsLock);
            TRANSLATE(ScrollLock, ScrollLock);
            TRANSLATE(NumLock, NumLock);
            TRANSLATE(PrintScreen, PrintScreen);
            TRANSLATE(Pause, Pause);
            TRANSLATE(Keypad0, Keypad0);
            TRANSLATE(Keypad1, Keypad1);
            TRANSLATE(Keypad2, Keypad2);
            TRANSLATE(Keypad3, Keypad3);
            TRANSLATE(Keypad4, Keypad4);
            TRANSLATE(Keypad5, Keypad5);
            TRANSLATE(Keypad6, Keypad6);
            TRANSLATE(Keypad7, Keypad7);
            TRANSLATE(Keypad8, Keypad8);
            TRANSLATE(Keypad9, Keypad9);
            TRANSLATE(KeypadDecimal, KeypadDecimal);
            TRANSLATE(KeypadDivide, KeypadDivide);
            TRANSLATE(KeypadMultiply, KeypadMultiply);
            TRANSLATE(KeypadSubtract, KeypadSubtract);
            TRANSLATE(KeypadAdd, KeypadAdd);
            TRANSLATE(KeypadEnter, KeypadEnter);
            TRANSLATE(KeypadEqual, KeypadEqual);
            TRANSLATE(LeftShift, LeftShift);
            TRANSLATE(LeftControl, LeftCtrl);
            TRANSLATE(LeftAlt, LeftAlt);
            TRANSLATE(LeftSuper, LeftSuper);
            TRANSLATE(RightShift, RightShift);
            TRANSLATE(RightControl, RightCtrl);
            TRANSLATE(RightAlt, RightAlt);
            TRANSLATE(RightSuper, RightSuper);
            TRANSLATE(Menu, Menu);
            TRANSLATE(_0, 0);
            TRANSLATE(_1, 1);
            TRANSLATE(_2, 2);
            TRANSLATE(_3, 3);
            TRANSLATE(_4, 4);
            TRANSLATE(_5, 5);
            TRANSLATE(_6, 6);
            TRANSLATE(_7, 7);
            TRANSLATE(_8, 8);
            TRANSLATE(_9, 9);
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
            return ImGuiKey_None;
        }
    }

    static ImGuiKey ToKeyModifier(Key key) {
        ZoneScoped;

        switch (key) {
        case Key::LeftShift:
        case Key::RightShift:
            return ImGuiMod_Shift;
        case Key::LeftAlt:
        case Key::RightAlt:
            return ImGuiMod_Alt;
        case Key::LeftControl:
        case Key::RightControl:
            return ImGuiMod_Ctrl;
        case Key::LeftSuper:
        case Key::RightSuper:
            return ImGuiMod_Super;
        default:
            return ImGuiKey_None;
        }
    }

    static bool ImGui_OnKey(const KeyEvent& event) {
        ZoneScoped;

        uint64_t id = event.GetView();
        auto viewport = ImGui::FindViewportByPlatformHandle((void*)id);

        if (viewport == nullptr) {
            return false; // we dont care
        }

        auto key = event.GetKey();
        bool pressed = event.IsPressed();

        auto converted = ToImGuiKey(key);
        if (converted == ImGuiKey_None) {
            // we dont have a mapping
            return false;
        }

        auto& io = ImGui::GetIO();
        io.AddKeyEvent(converted, pressed);

        auto mod = ToKeyModifier(key);
        if (mod != ImGuiKey_None) {
            io.AddKeyEvent(mod, pressed);
        }

        return io.WantCaptureKeyboard;
    }

    static bool ImGui_OnChar(const CharEvent& event) {
        ZoneScoped;

        uint64_t id = event.GetView();
        auto viewport = ImGui::FindViewportByPlatformHandle((void*)id);

        if (viewport == nullptr) {
            return false; // we dont care
        }

        auto& io = ImGui::GetIO();
        io.AddInputCharacter(event.GetCharacter());

        return io.WantCaptureKeyboard;
    }

    static bool ImGui_OnClose(const ViewClosedEvent& event) {
        ZoneScoped;

        uint64_t id = event.GetView();
        auto viewport = ImGui::FindViewportByPlatformHandle((void*)id);
        auto mainViewport = ImGui::GetMainViewport();

        if (viewport == nullptr || viewport->ID == mainViewport->ID) {
            return false; // don't care
        }

        viewport->PlatformRequestClose = true;
        return true;
    }

    void ImGuiHost::ProcessEvent(Event& event) {
        ZoneScoped;
        ImGui::SetCurrentContext(s_Data->Context);

        if (event.IsProcessed()) {
            return;
        }

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<ViewResizedEvent>(EventType::ViewResized, ImGui_OnResize);
        dispatcher.Dispatch<ViewMovedEvent>(EventType::ViewMoved, ImGui_OnMove);
        dispatcher.Dispatch<ViewFocusedEvent>(EventType::ViewFocused, ImGui_OnFocus);
        dispatcher.Dispatch<CursorEnteredEvent>(EventType::CursorEntered, ImGui_OnCursorEnter);
        dispatcher.Dispatch<CursorPositionEvent>(EventType::CursorPosition, ImGui_CursorPos);
        dispatcher.Dispatch<MouseButtonEvent>(EventType::MouseButton, ImGui_OnMouseButton);
        dispatcher.Dispatch<ScrollEvent>(EventType::Scroll, ImGui_OnScroll);
        dispatcher.Dispatch<KeyEvent>(EventType::Key, ImGui_OnKey);
        dispatcher.Dispatch<CharEvent>(EventType::Char, ImGui_OnChar);
        dispatcher.Dispatch<ViewClosedEvent>(EventType::ViewClosed, ImGui_OnClose);
    }

    void ImGuiHost::NewFrame() {
        ZoneScoped;
        ImGui::SetCurrentContext(s_Data->Context);

        Platform_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiHost::Render(const Ref<RenderTarget>& mainRenderTarget) {
        ZoneScoped;
        ImGui::SetCurrentContext(s_Data->Context);

        ImGui::Render();
        Renderer_RenderMainViewport(mainRenderTarget);

        auto& io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    ImGuiContext* ImGuiHost::GetContext() {
        if (!s_Data) {
            return nullptr;
        }

        return s_Data->Context;
    }

    ImTextureID ImGuiHost::GetTextureID(const Ref<Texture>& texture) {
        ZoneScoped;

        uint64_t textureID = texture->GetID();
        if (s_Data->TextureMap.contains(textureID)) {
            return s_Data->TextureMap.at(textureID);
        }

        // see assets/shaders/ImGui.glsl
        auto allocation = Renderer::CreateAllocation(s_Data->ImGuiShader);
        if (!allocation->Bind("u_Texture", texture)) {
            throw std::runtime_error("Failed to bind texture to ImGui allocation!");
        }

        ImTextureID newID = s_Data->TextureID++;
        s_Data->TextureMap[textureID] = newID;
        s_Data->TextureAllocations[newID] = allocation;

        return newID;
    }

    void ImGuiHost::FreeTexture(uint64_t id) {
        ZoneScoped;
        if (!s_Data || !s_Data->TextureMap.contains(id)) {
            return;
        }

        ImTextureID imguiID = s_Data->TextureMap.at(id);
        s_Data->TextureAllocations.erase(imguiID);
        s_Data->TextureMap.erase(id);
    }

    void ImGuiHost::FreeRenderTarget(uint32_t id) {
        ZoneScoped;
        if (!s_Data || !s_Data->MainViewportPipelines.contains(id)) {
            return;
        }

        s_Data->MainViewportPipelines.erase(id);
    }

    static void Platform_CreateWindow(ImGuiViewport* viewport) {
        ZoneScoped;

        ViewSize size;
        size.Width = (uint32_t)viewport->Size.x;
        size.Height = (uint32_t)viewport->Size.y;

        ViewCreationOptions options;
        options.Visible = false;
        options.Focused = false;
        options.FocusOnShow = false;

        if ((viewport->Flags & ImGuiViewportFlags_NoDecoration) != 0) {
            options.Decorated = false;
        }

        if ((viewport->Flags & ImGuiViewportFlags_TopMost) != 0) {
            options.Floating = true;
        }

        auto platformData = new ViewportPlatformData;
        platformData->ViewportView = Platform::CreateView("ImGui Viewport", size, options);
        platformData->IgnoreResizeFrame = -1;
        platformData->IgnoreMoveFrame = -1;

        viewport->PlatformUserData = platformData;
        viewport->PlatformHandle = (void*)platformData->ViewportView->GetID();

#ifdef FUUJIN_PLATFORM_linux
        // see imgui_impl_glfw.cpp @ line 1199
        s_Data->IgnoreMouseUpOnFocusLoss = true;
#endif
    }

    static void Platform_DestroyWindow(ImGuiViewport* viewport) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;

        // view is ref-counted
        delete platformData;

        viewport->PlatformUserData = nullptr;
        viewport->PlatformHandle = nullptr;
    }

    static void Platform_ShowWindow(ImGuiViewport* viewport) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        platformData->ViewportView->Show();
    }

    static ImVec2 Platform_GetWindowPos(ImGuiViewport* viewport) {
        ZoneScoped;

        uint32_t x, y;
        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        platformData->ViewportView->GetPosition(x, y);

        return ImVec2((float)x, (float)y);
    }

    static void Platform_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        platformData->ViewportView->SetPosition((uint32_t)pos.x, (uint32_t)pos.y);
        platformData->IgnoreMoveFrame = ImGui::GetFrameCount();
    }

    static ImVec2 Platform_GetWindowSize(ImGuiViewport* viewport) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        auto size = platformData->ViewportView->GetSize();

        return ImVec2((float)size.Width, (float)size.Height);
    }

    static void Platform_SetWindowSize(ImGuiViewport* viewport, ImVec2 size) {
        ZoneScoped;

        ViewSize newSize;
        newSize.Width = (uint32_t)size.x;
        newSize.Height = (uint32_t)size.y;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        platformData->ViewportView->SetSize(newSize);
        platformData->IgnoreResizeFrame = ImGui::GetFrameCount();
    }

    static void Platform_GetViewFramebufferScale(const Ref<View>& view, ImVec2* windowSize,
                                                 ImVec2* framebufferScale) {
        ZoneScoped;

        auto size = view->GetSize();
        auto framebufferSize = view->GetFramebufferSize();

        if (windowSize != nullptr) {
            windowSize->x = (float)size.Width;
            windowSize->y = (float)size.Height;
        }

        if (framebufferScale != nullptr) {
            if (size.Width > 0 && size.Height > 0) {
                framebufferScale->x = (float)framebufferSize.Width / size.Width;
                framebufferScale->y = (float)framebufferSize.Height / size.Height;
            } else {
                framebufferScale->x = framebufferScale->y = 1.f;
            }
        }
    }

    static ImVec2 Platform_GetWindowFramebufferScale(ImGuiViewport* viewport) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;

        ImVec2 scale;
        Platform_GetViewFramebufferScale(platformData->ViewportView, nullptr, &scale);

        return scale;
    }

    static void Platform_SetWindowTitle(ImGuiViewport* viewport, const char* title) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        platformData->ViewportView->SetTitle(title);
    }

    static void Platform_SetWindowFocus(ImGuiViewport* viewport) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        platformData->ViewportView->Focus();
    }

    static bool Platform_GetWindowFocus(ImGuiViewport* viewport) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        return platformData->ViewportView->IsFocused();
    }

    static bool Platform_GetWindowMinimized(ImGuiViewport* viewport) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        return platformData->ViewportView->IsMinimized();
    }

    static void Platform_SetWindowAlpha(ImGuiViewport* viewport, float alpha) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        platformData->ViewportView->SetAlpha(alpha);
    }

    static const char* Platform_GetClipboardText(ImGuiContext* context) {
        ZoneScoped;

        s_Data->Clipboard = s_Data->MainView->GetClipboardString();
        return s_Data->Clipboard.c_str();
    }

    static void Platform_SetClipboardText(ImGuiContext* context, const char* text) {
        ZoneScoped;

        s_Data->Clipboard = text;
        s_Data->MainView->SetClipboardString(s_Data->Clipboard);
    }

    void ImGuiHost::Platform_Init() {
        ZoneScoped;

        ImGuiIO& io = ImGui::GetIO();
        ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();

        s_Data->PlatformName = Platform::GetName();
        io.BackendPlatformName = s_Data->PlatformName.c_str();

        s_Data->IgnoreMouseUp = false;
        s_Data->IgnoreMouseUpOnFocusLoss = false;

        if (Platform::HasCursors()) {
            io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        }

        if (Platform::CanSetCursorPos()) {
            io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
        }

        if (Platform::HasViewports()) {
            io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
        }

        if (Platform::HasViewHovered()) {
            io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
        }

        platformIO.Platform_CreateWindow = Platform_CreateWindow;
        platformIO.Platform_DestroyWindow = Platform_DestroyWindow;
        platformIO.Platform_ShowWindow = Platform_ShowWindow;
        platformIO.Platform_SetWindowPos = Platform_SetWindowPos;
        platformIO.Platform_GetWindowPos = Platform_GetWindowPos;
        platformIO.Platform_SetWindowSize = Platform_SetWindowSize;
        platformIO.Platform_GetWindowSize = Platform_GetWindowSize;
        platformIO.Platform_GetWindowFramebufferScale = Platform_GetWindowFramebufferScale;
        platformIO.Platform_SetWindowTitle = Platform_SetWindowTitle;
        platformIO.Platform_SetWindowFocus = Platform_SetWindowFocus;
        platformIO.Platform_GetWindowFocus = Platform_GetWindowFocus;
        platformIO.Platform_GetWindowMinimized = Platform_GetWindowMinimized;
        platformIO.Platform_SetWindowAlpha = Platform_SetWindowAlpha;

        platformIO.Platform_GetClipboardTextFn = Platform_GetClipboardText;
        platformIO.Platform_SetClipboardTextFn = Platform_SetClipboardText;

        auto& app = Application::Get();
        s_Data->MainView = app.GetView();

        auto mainViewportData = new ViewportPlatformData;
        mainViewportData->ViewportView = s_Data->MainView;

        auto viewport = ImGui::GetMainViewport();
        viewport->PlatformUserData = mainViewportData;
        viewport->PlatformHandle = (void*)s_Data->MainView->GetID();
    }

    static Ref<Pipeline> Renderer_CreatePipeline(Ref<RenderTarget> target) {
        ZoneScoped;

        Pipeline::Spec spec;
        spec.PipelineType = Pipeline::Type::Graphics;
        spec.Target = target;
        spec.PipelineShader = s_Data->ImGuiShader;
        spec.DisableCulling = true;
        spec.Depth.Test = false;
        spec.Depth.Write = false;
        spec.Blending = ColorBlending::Default;

        return s_Data->RendererContext->CreatePipeline(spec);
    }

    static void Renderer_CreateWindow(ImGuiViewport* viewport) {
        ZoneScoped;

        auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
        auto rendererData = new ViewportRendererData;

        rendererData->ViewSwapchain =
            s_Data->RendererContext->CreateSwapchain(platformData->ViewportView);

        rendererData->ViewPipeline = Renderer_CreatePipeline(rendererData->ViewSwapchain);

        viewport->RendererUserData = rendererData;
    }

    static void Renderer_DestroyWindow(ImGuiViewport* viewport) {
        ZoneScoped;

        s_Data->RendererContext->GetDevice()->Wait();
        Renderer::GetGraphicsQueue()->Clear();

        auto rendererData = (ViewportRendererData*)viewport->RendererUserData;

        // all ref-counted
        delete rendererData;

        viewport->RendererUserData = nullptr;
    }

    void ImGuiHost::Renderer_Init() {
        ZoneScoped;

        s_Data->RendererContext = Renderer::GetContext();
        if (s_Data->RendererContext.IsEmpty()) {
            FUUJIN_ERROR("Renderer not initialized!");
        }

        auto& library = Renderer::GetShaderLibrary();
        s_Data->ImGuiShader = library.Get("fuujin/shaders/ImGui.glsl");

        s_Data->TextureID = 1;

        // we do not own a swapchain for the main viewport
        // also, we manage pipelines on a per-render target basis
        // we do not need to store one in the renderer data for the viewport
        auto mainViewport = ImGui::GetMainViewport();
        mainViewport->RendererUserData = new ViewportRendererData;

        const auto& api = Renderer::GetAPI();
        s_Data->RendererName = api.Name;

        auto& io = ImGui::GetIO();
        auto& platformIO = ImGui::GetPlatformIO();

        io.BackendRendererName = s_Data->RendererName.c_str();
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

        platformIO.Renderer_CreateWindow = Renderer_CreateWindow;
        platformIO.Renderer_DestroyWindow = Renderer_DestroyWindow;
        platformIO.Renderer_RenderWindow = Renderer_RenderViewport;

        // we dont need to set Renderer_SwapBuffers
        // present is already handled in Renderer_RenderWindow
    }

    void ImGuiHost::Platform_Shutdown() {
        ZoneScoped;

        // "DestroyWindow" just destroys platform data and removes reference
        auto mainViewport = ImGui::GetMainViewport();
        Platform_DestroyWindow(mainViewport);
    }

    void ImGuiHost::Renderer_Shutdown() {
        ZoneScoped;

        // again, "DestroyWindow" just destroys platform data and removes references
        auto mainViewport = ImGui::GetMainViewport();
        Renderer_DestroyWindow(mainViewport);
    }

    void ImGuiHost::Platform_NewFrame() {
        ZoneScoped;

        auto& io = ImGui::GetIO();
        Platform_GetViewFramebufferScale(s_Data->MainView, &io.DisplaySize,
                                         &io.DisplayFramebufferScale);

        Platform_UpdateMonitors();

        auto now = std::chrono::high_resolution_clock::now();
        if (s_Data->Time.has_value()) {
            auto delta = now - s_Data->Time.value();
            io.DeltaTime = std::chrono::duration_cast<std::chrono::duration<float>>(delta).count();
        } else {
            io.DeltaTime = 1.f / 60.f;
        }

        s_Data->Time = now;

        Platform_UpdateMouse();
        Platform_UpdateCursor();

        // todo: gamepads
    }

    void ImGuiHost::Platform_UpdateMouse() {
        ZoneScoped;

        auto& io = ImGui::GetIO();
        auto& platformIO = ImGui::GetPlatformIO();

        ImGuiID mouseViewport = ImGui::GetMainViewport()->ID;
        ImVec2 previousPosition = io.MousePos;

        for (int i = 0; i < platformIO.Viewports.Size; i++) {
            auto viewport = platformIO.Viewports[i];
            auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;
            auto view = platformData->ViewportView;

            if (view->IsFocused()) {
                if (io.WantSetMousePos) {
                    double x = (double)(previousPosition.x - viewport->Pos.x);
                    double y = (double)(previousPosition.y - viewport->Pos.y);
                    view->SetCursorPosition(x, y);
                }

                if (!s_Data->MouseView.has_value()) {
                    double x, y;
                    view->GetCursorPosition(x, y);

                    if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
                        uint32_t windowX, windowY;
                        view->GetPosition(windowX, windowY);

                        x += (double)windowX;
                        y += (double)windowY;
                    }

                    ImVec2 mousePos((float)x, (float)y);
                    s_Data->LastValidMousePos = mousePos;
                    io.AddMousePosEvent(mousePos.x, mousePos.y);
                }
            }

            bool noInput = (viewport->Flags & ImGuiViewportFlags_NoInputs) != 0;
            view->SetPassthrough(noInput);

            if (view->IsHovered()) {
                mouseViewport = viewport->ID;
            }
        }

        if (Platform::HasViewHovered()) {
            io.AddMouseViewportEvent(mouseViewport);
        }
    }

    void ImGuiHost::Platform_UpdateCursor() {
        ZoneScoped;

        auto& io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) != 0 ||
            s_Data->MainView->IsCursorDisabled()) {
            return;
        }

        auto imguiCursor = ImGui::GetMouseCursor();
        auto& platformIO = ImGui::GetPlatformIO();

        Cursor cursor;
        if (imguiCursor == ImGuiMouseCursor_None || io.MouseDrawCursor) {
            cursor = Cursor::None;
        } else {
            switch (imguiCursor) {
            case ImGuiMouseCursor_TextInput:
                cursor = Cursor::TextInput;
                break;
            case ImGuiMouseCursor_ResizeAll:
                cursor = Cursor::ResizeAll;
                break;
            case ImGuiMouseCursor_ResizeNS:
                cursor = Cursor::ResizeNS;
                break;
            case ImGuiMouseCursor_ResizeEW:
                cursor = Cursor::ResizeEW;
                break;
            case ImGuiMouseCursor_ResizeNESW:
                cursor = Cursor::ResizeNESW;
                break;
            case ImGuiMouseCursor_ResizeNWSE:
                cursor = Cursor::ResizeNWSE;
                break;
            case ImGuiMouseCursor_Hand:
                cursor = Cursor::Hand;
                break;
            case ImGuiMouseCursor_NotAllowed:
                cursor = Cursor::NotAllowed;
                break;
            default:
                cursor = Cursor::Arrow;
                break;
            }
        }

        for (int i = 0; i < platformIO.Viewports.Size; i++) {
            auto viewport = platformIO.Viewports[i];
            auto platformData = (ViewportPlatformData*)viewport->PlatformUserData;

            platformData->ViewportView->SetCursor(cursor);
        }
    }

    void ImGuiHost::Platform_UpdateMonitors() {
        ZoneScoped;

        std::vector<MonitorInfo> monitors;
        if (!Platform::QueryMonitors(monitors)) {
            return;
        }

        auto& platformIO = ImGui::GetPlatformIO();
        platformIO.Monitors.clear();

        for (const auto& monitor : monitors) {
            ImGuiPlatformMonitor result;

            result.MainPos = ImVec2((float)monitor.MainPos.x, (float)monitor.MainPos.y);
            result.MainSize = ImVec2((float)monitor.MainSize.x, (float)monitor.MainSize.y);
            result.WorkPos = ImVec2((float)monitor.WorkPos.x, (float)monitor.WorkPos.y);
            result.WorkSize = ImVec2((float)monitor.WorkSize.x, (float)monitor.WorkSize.y);
            result.DpiScale = monitor.Scale;
            result.PlatformHandle = monitor.Handle;

            platformIO.Monitors.push_back(result);
        }
    }

    void ImGuiHost::Renderer_RenderViewport(ImGuiViewport* viewport, void* renderArg) {
        ZoneScoped;

        auto rendererData = (ViewportRendererData*)viewport->RendererUserData;
        Renderer::PushRenderTarget(rendererData->ViewSwapchain);

        Renderer_RenderDrawData(viewport, rendererData->ViewPipeline);

        Renderer::PopRenderTarget();
    }

    void ImGuiHost::Renderer_RenderMainViewport(const Ref<RenderTarget>& target) {
        ZoneScoped;

        uint64_t id = target->GetID();
        if (!s_Data->MainViewportPipelines.contains(id)) {
            s_Data->MainViewportPipelines[id] = Renderer_CreatePipeline(target);
        }

        auto pipeline = s_Data->MainViewportPipelines.at(id);
        auto drawData = ImGui::GetDrawData();

        Renderer_RenderDrawData(ImGui::GetMainViewport(), pipeline);
    }

    struct ImGuiBufferLoadData {
        Buffer Data;
        Ref<DeviceBuffer> Destination;
    };

    static void RT_LoadImGuiBuffer(ImGuiBufferLoadData* data) {
        ZoneScoped;

        DeviceBuffer::Spec stagingSpec;
        stagingSpec.Size = data->Data.GetSize();
        stagingSpec.BufferUsage = DeviceBuffer::Usage::Staging;
        stagingSpec.QueueOwnership = { QueueType::Transfer };

        auto context = Renderer::GetContext();
        auto staging = context->CreateBuffer(stagingSpec);

        auto mapped = staging->RT_Map();
        Buffer::Copy(data->Data, mapped, data->Data.GetSize());
        staging->RT_Unmap();

        auto queue = context->GetQueue(QueueType::Transfer);
        auto& cmdlist = queue->RT_Get();

        cmdlist.AddDependency(staging);
        cmdlist.AddDependency(data->Destination);

        cmdlist.RT_Begin();
        staging->RT_CopyToBuffer(cmdlist, data->Destination);
        cmdlist.RT_End();

        queue->RT_Submit(cmdlist);

        delete data;
    }

    template <typename _Ty>
    static void LoadImGuiBuffer(Ref<DeviceBuffer>& buffer, const std::vector<_Ty>& data,
                                DeviceBuffer::Usage usage) {
        size_t dataSize = data.size() * sizeof(_Ty);
        if (buffer.IsEmpty() || buffer->GetSpec().Size < dataSize) {
            DeviceBuffer::Spec spec;
            spec.Size = dataSize;
            spec.QueueOwnership = { QueueType::Graphics, QueueType::Transfer };
            spec.BufferUsage = usage;

            auto context = Renderer::GetContext();
            buffer = context->CreateBuffer(spec);
        }

        auto loadData = new ImGuiBufferLoadData;
        loadData->Destination = buffer;
        loadData->Data = Buffer::Wrapper(data).Copy(); // we cant guarantee lifetime

        Renderer::Submit([loadData]() { RT_LoadImGuiBuffer(loadData); });
    }

    static void Renderer_UpdateTexture(ImTextureData* tex) {
        ZoneScoped;

        if (tex->Status == ImTextureStatus_OK) {
            return;
        }

        Scissor scissor;
        if (tex->Status == ImTextureStatus_WantCreate) {
            scissor.X = 0;
            scissor.Y = 0;
            scissor.Width = (uint32_t)tex->Width;
            scissor.Height = (uint32_t)tex->Height;
        } else {
            scissor.X = (int32_t)tex->UpdateRect.x;
            scissor.Y = (int32_t)tex->UpdateRect.y;
            scissor.Width = (uint32_t)tex->UpdateRect.w;
            scissor.Height = (uint32_t)tex->UpdateRect.h;
        }

        Buffer data;
        if (tex->Pixels != nullptr) {
            size_t pitch = (size_t)scissor.Width * tex->BytesPerPixel;
            size_t dataSize = pitch * scissor.Height;
            data = Buffer(dataSize);

            for (uint32_t y = 0; y < scissor.Height; y++) {
                void* row = tex->GetPixelsAt((int)scissor.X, (int)(scissor.Y + y));
                auto rowData = Buffer::Wrapper(row, pitch);

                Buffer::Copy(rowData, data.Slice(pitch * y));
            }
        }

        switch (tex->Status) {
        case ImTextureStatus_WantCreate: {
            Texture::Format format;
            switch (tex->Format) {
            case ImTextureFormat_RGBA32:
                format = Texture::Format::RGBA8;
                break;
            case ImTextureFormat_Alpha8:
                format = Texture::Format::A8;
                break;
            default:
                throw std::runtime_error("Invalid texture format!");
            }

            auto texture =
                Renderer::CreateTexture((uint32_t)tex->Width, (uint32_t)tex->Height, format, data);

            tex->SetTexID(ImGuiHost::GetTextureID(texture));
            tex->SetStatus(ImTextureStatus_OK);

            s_Data->ImGuiTextures[tex->UniqueID] = texture;
        } break;
        case ImTextureStatus_WantUpdates: {
            auto texture = s_Data->ImGuiTextures.at(tex->UniqueID);
            Renderer::UpdateTexture(texture, data, scissor);

            tex->SetStatus(ImTextureStatus_OK);
        } break;
        case ImTextureStatus_WantDestroy: {
            auto texture = s_Data->ImGuiTextures.at(tex->UniqueID);
            ImGuiHost::FreeTexture(texture->GetID());

            s_Data->ImGuiTextures.erase(tex->UniqueID);

            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        } break;
        default:
            // nothing
            break;
        }
    }

    void ImGuiHost::Renderer_RenderDrawData(ImGuiViewport* viewport,
                                            const Ref<Pipeline>& pipeline) {
        ZoneScoped;

        auto rendererData = (ViewportRendererData*)viewport->RendererUserData;
        auto renderTarget = pipeline->GetSpec().Target;

        auto drawData = viewport->DrawData;
        if (drawData->TotalVtxCount == 0 || drawData->TotalIdxCount == 0) {
            return;
        }

        if (drawData->Textures != nullptr) {
            const auto& textures = *drawData->Textures;
            for (int i = 0; i < textures.size(); i++) {
                auto tex = textures[i];
                Renderer_UpdateTexture(tex);
            }
        }

        if (rendererData->RenderBuffers.size() < drawData->CmdListsCount) {
            rendererData->RenderBuffers.resize(drawData->CmdListsCount);
        }

        ImVec2 clipOffset = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        glm::vec2 scale;
        scale.x = 2.f / drawData->DisplaySize.x;
        scale.y = 2.f / drawData->DisplaySize.y;

        glm::vec2 translation;
        translation.x = -1.f - drawData->DisplayPos.x * scale.x;
        translation.y = -1.f - drawData->DisplayPos.y * scale.y;

        auto pushConstantsType = s_Data->ImGuiShader->GetPushConstants()->GetType();
        ShaderBuffer pushConstants(pushConstantsType);

        pushConstants.Set("Scale", scale);
        pushConstants.Set("Translation", translation);

        IndexedRenderCall call;
        call.RenderPipeline = pipeline;
        call.PushConstants = pushConstants.GetBuffer();
        call.Flip = false;

        for (int i = 0; i < drawData->CmdListsCount; i++) {
            auto cmdlist = drawData->CmdLists[i];
            auto& buffers = rendererData->RenderBuffers[i];

            std::vector<ImGuiVertex> vertices(cmdlist->VtxBuffer.size());
            std::vector<uint32_t> indices(cmdlist->IdxBuffer.size());

            for (int j = 0; j < cmdlist->VtxBuffer.size(); j++) {
                const auto& drawVertex = cmdlist->VtxBuffer[j];
                auto& vertex = vertices[j];

                vertex.Position = glm::vec2(drawVertex.pos.x, drawVertex.pos.y);
                vertex.UV = glm::vec2(drawVertex.uv.x, drawVertex.uv.y);

                auto color = ImGui::ColorConvertU32ToFloat4(drawVertex.col);
                vertex.Color = glm::vec4(color.x, color.y, color.z, color.w);
            }

            for (int j = 0; j < cmdlist->IdxBuffer.size(); j++) {
                ImDrawIdx drawIndex = cmdlist->IdxBuffer[j];
                indices[j] = (uint32_t)drawIndex;
            }

            LoadImGuiBuffer(buffers.VertexBuffer, vertices, DeviceBuffer::Usage::Vertex);
            LoadImGuiBuffer(buffers.IndexBuffer, indices, DeviceBuffer::Usage::Index);

            call.VertexBuffers = { buffers.VertexBuffer };
            call.IndexBuffer = buffers.IndexBuffer;

            for (int j = 0; j < cmdlist->CmdBuffer.size(); j++) {
                const auto& cmd = cmdlist->CmdBuffer[j];
                const auto& clip = cmd.ClipRect;

                // min
                float clipX = (clip.x - clipOffset.x) * clipScale.x;
                float clipY = (clip.y - clipOffset.y) * clipScale.y;

                // max
                float clipZ = (clip.z - clipOffset.x) * clipScale.x;
                float clipW = (clip.w - clipOffset.y) * clipScale.y;

                Scissor scissor;
                scissor.X = (int32_t)clipX;
                scissor.Y = (int32_t)clipY;
                scissor.Width = (uint32_t)(clipZ - clipX);
                scissor.Height = (uint32_t)(clipW - clipY);

                ImTextureID id = cmd.GetTexID();
                auto allocation = s_Data->TextureAllocations.at(id);

                call.ScissorRect = scissor;
                call.Resources = { allocation };
                call.VertexOffset = (int32_t)cmd.VtxOffset;
                call.IndexOffset = (uint32_t)cmd.IdxOffset;
                call.IndexCount = (uint32_t)cmd.ElemCount;

                Renderer::RenderIndexed(call);
            }
        }
    }
} // namespace fuujin
