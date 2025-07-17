#include "fuujinpch.h"
#include "fuujin/imgui/ImGuiLayer.h"

#include "fuujin/imgui/ImGuiHost.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    ImGuiLayer::ImGuiLayer() {
        ZoneScoped;

        ImGuiHost::Init();
    }

    ImGuiLayer::~ImGuiLayer() {
        ZoneScoped;

        ImGuiHost::Shutdown();
    }

    void ImGuiLayer::ProcessEvent(Event& event) {
        ZoneScoped;

        ImGuiHost::ProcessEvent(event);
    }

    void ImGuiLayer::PreUpdate(Duration delta) {
        ZoneScoped;

        ImGuiHost::NewFrame();
    }

    void ImGuiLayer::PostUpdate(Duration delta) {
        ZoneScoped;

        auto context = Renderer::GetContext();
        auto swapchain = context->GetSwapchain();

        ImGuiHost::Render(swapchain);
    }
} // namespace fuujin
