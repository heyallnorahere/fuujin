#include "fuujinpch.h"
#include "fuujin/core/Application.h"

#include "fuujin/core/Event.h"
#include "fuujin/core/Layer.h"
#include "fuujin/core/View.h"

namespace fuujin {
    static std::unique_ptr<Application> s_App;

    Application& Application::Get() {
        if (!s_App) {
            throw std::runtime_error("No application is present!");
        }

        return *s_App;
    }

    int Application::Run(const std::function<void()>& initialization) {
        ZoneScoped;
        if (s_App) {
            return -1;
        }

        s_App = std::unique_ptr<Application>(new Application);
        initialization();

        while (!s_App->GetView().IsClosed()) {
            s_App->Update();
        }

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