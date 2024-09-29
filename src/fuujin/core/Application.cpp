#include "fuujinpch.h"
#include "fuujin/core/Application.h"

#include "fuujin/core/Event.h"
#include "fuujin/core/Layer.h"
#include "fuujin/core/View.h"
#include "fuujin/renderer/Renderer.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace fuujin {
    static Application* s_App = nullptr;

    static const std::string s_LogPattern = "[%s:%#] [%^%l%$] %v";
    spdlog::logger s_Logger = spdlog::logger("fuujin");

    Application& Application::Get() {
        if (!s_App) {
            throw std::runtime_error("No application is present!");
        }

        return *s_App;
    }

    int Application::Run(const std::function<void()>& initialization) {
        ZoneScoped;

        spdlog::level::level_enum level;
#ifdef FUUJIN_IS_DEBUG
        level = spdlog::level::debug;
#else
        level = spdlog::level::info;
#endif

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(level);
        consoleSink->set_pattern(s_LogPattern);

        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/fuujin.log", 10 * 1024 * 1024, 3);
        consoleSink->set_level(spdlog::level::trace);
        consoleSink->set_pattern("[%x %X] " + s_LogPattern);

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
        while (!app.GetView().IsClosed()) {
            app.Update();
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
        s_App = this;

        m_Data = new ApplicationData;
        m_Data->LastTimestamp = std::chrono::high_resolution_clock::now();
        m_Data->AppView = View::Create("風神", { 1600, 900 });

        Renderer::Init();
    }

    Application::~Application() {
        Renderer::Shutdown();

        delete m_Data;
        s_App = nullptr;
    }

    void Application::ProcessEvent(Event& event) {
        ZoneScoped;

        // todo: process through special utils

        for (const auto& layer : m_Data->LayerStack) {
            if (event.IsProcessed()) {
                return;
            }

            event.SetProcessed(layer->ProcessEvent(event));
        }
    }

    View& Application::GetView() { return *m_Data->AppView; }
    const View& Application::GetView() const { return *m_Data->AppView; }

    void Application::PushLayer(Layer* layer) {
        m_Data->LayerStack.insert(m_Data->LayerStack.begin(), std::unique_ptr<Layer>(layer));
    }

    void Application::Update() {
        ZoneScoped;

        auto now = std::chrono::high_resolution_clock::now();
        auto delta = std::chrono::duration_cast<Duration>(now - m_Data->LastTimestamp);
        m_Data->LastTimestamp = now;

        m_Data->AppView->Update();
        for (const auto& layer : m_Data->LayerStack) {
            layer->Update(delta);
        }
    }
} // namespace fuujin