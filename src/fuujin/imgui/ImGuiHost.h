#pragma once

#include <imgui.h>

#include "fuujin/renderer/Texture.h"
#include "fuujin/renderer/Framebuffer.h"

namespace fuujin {
    class CommandList;
    class ImGuiHost {
    public:
        ImGuiHost() = delete;

        static void Init();
        static void Shutdown();

        static void NewFrame();
        static void Render(CommandList& cmdlist, const Ref<RenderTarget>& mainRenderTarget);

        static ImGuiContext* GetContext();

        static ImTextureID GetTextureID(const Ref<Texture>& texture);
        static void FreeTexture(uint64_t id);

        static void FreeRenderTarget(uint32_t id);

    private:
        static void Platform_Init();
        static void Renderer_Init();

        static void Platform_NewFrame();
        static void Platform_UpdateMouse();
        static void Platform_UpdateCursor();
        static void Platform_UpdateMonitors();

        static void Renderer_RenderWindow(ImGuiViewport* viewport, void* renderArg);

        static void Renderer_RenderDrawData(ImDrawData* data, CommandList& cmdlist,
                                   const Ref<RenderTarget>& mainRenderTarget);
    };
} // namespace fuujin