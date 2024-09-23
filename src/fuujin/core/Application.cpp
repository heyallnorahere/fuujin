#include "fuujinpch.h"
#include "fuujin/core/Application.h"

#include "fuujin/core/Event.h"
#include "fuujin/core/Layer.h"
#include "fuujin/core/View.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace fuujin {
    static std::unique_ptr<Application> s_App;

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
        level = spdlog::level::trace;
#else
        level = spdlog::level::warn;
#endif

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(level);
        consoleSink->set_pattern("[%s:%#] [%^%l%$] %v");
        
        s_Logger.sinks().push_back(consoleSink);
        s_Logger.set_level(spdlog::level::trace);

        if (s_App) {
            FUUJIN_INFO("App already running; aborting second launch");
            return -1;
        }

        s_App = std::unique_ptr<Application>(new Application);
        initialization();

        FUUJIN_INFO("Initialized. Launching...");
        while (!s_App->GetView().IsClosed()) {
            s_App->Update();
        }

        FUUJIN_INFO("Exiting...");
        s_App.reset();

        return 0;
    }

    struct ApplicationData {
        std::unique_ptr<View> AppView;
        std::vector<std::unique_ptr<Layer>> LayerStack;

        std::chrono::high_resolution_clock::time_point LastTimestamp;
    };

    Application::Application() {
        m_Data = new ApplicationData;
        m_Data->LastTimestamp = std::chrono::high_resolution_clock::now();
        m_Data->AppView = std::unique_ptr<View>(View::Create("風神", { 1600, 900 }));
    }

    Application::~Application() { delete m_Data; }

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