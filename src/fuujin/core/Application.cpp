#include "fuujinpch.h"
#include "fuujin/core/Application.h"

#include "fuujin/core/Platform.h"
#include "fuujin/core/Event.h"
#include "fuujin/core/Layer.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/GraphicsContext.h"

#include "fuujin/asset/AssetManager.h"
#include "fuujin/asset/ModelSource.h"

#include "fuujin/animation/Animation.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace fuujin {
    static Application* s_App = nullptr;

    spdlog::logger s_Logger = spdlog::logger("fuujin");

    Application& Application::Get() {
        if (!s_App) {
            throw std::runtime_error("No application is present!");
        }

        return *s_App;
    }

    int Application::Run(const std::function<void()>& initialization) {
        spdlog::level::level_enum level;
#ifdef FUUJIN_IS_DEBUG
        level = spdlog::level::debug;
#else
        level = spdlog::level::info;
#endif

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(level);
        consoleSink->set_pattern("[%s:%# %!] [%^%l%$] %v");

        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/fuujin.log",
                                                                               10 * 1024 * 1024, 3);
        fileSink->set_level(spdlog::level::trace);
        fileSink->set_pattern("[%x %X] [%g:%# %!] [%l] %v");

        auto& sinks = s_Logger.sinks();
        sinks.insert(sinks.end(), { consoleSink, fileSink });
        s_Logger.set_level(spdlog::level::trace);

        if (s_App) {
            FUUJIN_INFO("App already running; aborting second launch");
            return -1;
        }

        Application app;
        initialization();

        FUUJIN_INFO("Initialized. Launching...");
        while (!app.GetView()->IsClosed()) {
            app.Loop();
        }

        FUUJIN_INFO("Exiting...");
        return 0;
    }

    struct ApplicationData {
        Ref<View> AppView;
        std::vector<std::unique_ptr<Layer>> LayerStack;

        std::chrono::high_resolution_clock::time_point LastTimestamp;
    };

    Application::Application() {
        ZoneScoped;

        s_App = this;

        Platform::Init();

        m_Data = new ApplicationData;
        m_Data->LastTimestamp = std::chrono::high_resolution_clock::now();
        m_Data->AppView = Platform::CreateView("風神", { 1600, 900 });

        Renderer::Init();

        AssetManager::RegisterAssetType<TextureSerializer>();
        AssetManager::RegisterAssetType<MaterialSerializer>();
        AssetManager::RegisterAssetType<ModelSourceSerializer>();
        AssetManager::RegisterAssetType<ModelSerializer>();
        AssetManager::RegisterAssetType<AnimationSerializer>();

        AssetManager::LoadDirectory("assets", "fuujin");
    }

    Application::~Application() {
        ZoneScoped;

        m_Data->LayerStack.clear();
        AssetManager::Clear();
        Renderer::Shutdown();

        m_Data->AppView.Reset();
        Platform::Shutdown();

        delete m_Data;
        s_App = nullptr;
    }

    static std::string GetEventName(EventType type) {
        switch (type) {
        case EventType::ViewResized:
            return "ViewResized";
        case EventType::ViewFocused:
            return "ViewFocused";
        case EventType::CursorEntered:
            return "CursorEntered";
        case EventType::CursorPosition:
            return "CursorPosition";
        case EventType::MouseButton:
            return "MouseButton";
        case EventType::Scroll:
            return "Scroll";
        case EventType::Key:
            return "Key";
        case EventType::Char:
            return "Char";
        case EventType::MonitorUpdate:
            return "MonitorUpdate";
        case EventType::ViewClosed:
            return "ViewClosed";
        default:
            return "Unknown";
        }
    }

    void Application::ProcessEvent(Event& event) {
        ZoneScoped;

        if (s_App == nullptr) {
            FUUJIN_WARN("Attempted to process event with no application present!");
            return;
        }

        auto name = GetEventName(event.GetType());
        FUUJIN_TRACE("Processing event of type {}", name.c_str());

        s_App->OnEvent(event);
    }

    Ref<View> Application::GetView() const { return m_Data->AppView; }

    void Application::PushLayer(Layer* layer) {
        ZoneScoped;
        m_Data->LayerStack.insert(m_Data->LayerStack.begin(), std::unique_ptr<Layer>(layer));
    }

    void Application::Loop() {
        ZoneScoped;

        auto now = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration_cast<Duration>(now - m_Data->LastTimestamp);
        m_Data->LastTimestamp = now;

        Update(delta);
    }

    void Application::Update(Duration delta) {
        ZoneScoped;

        Renderer::NewFrame();
        Renderer::PushRenderTarget(Renderer::GetContext()->GetSwapchain());

        for (const auto& layer : m_Data->LayerStack) {
            layer->Update(delta);
        }

        Renderer::PopRenderTarget();
        Renderer::Wait();

        m_Data->AppView->Update();
    }

    void Application::OnEvent(Event& event) {
        ZoneScoped;

        Renderer::ProcessEvent(event);
        if (event.IsProcessed()) {
            return;
        }

        for (const auto& layer : m_Data->LayerStack) {
            if (event.IsProcessed()) {
                return;
            }

            layer->ProcessEvent(event);
        }
    }
} // namespace fuujin
