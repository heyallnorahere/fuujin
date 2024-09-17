#include "fuujinpch.h"
#include "fuujin/Application.h"

#include "fuujin/Event.h"
#include "fuujin/Layer.h"
#include "fuujin/View.h"

namespace fuujin {
    static std::unique_ptr<Application> s_App;

    Application& Application::Get() {
        if (!s_App) {
            throw std::runtime_error("No application is present!");
        }

        return *s_App;
    }

    int Application::Run(int argc, const char** argv) {
        ZoneScoped;
        if (s_App) {
            return -1;
        }

        s_App = std::unique_ptr<Application>(new Application);
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

    template <typename _Ty, typename... Args>
    static void PushLayer(ApplicationData* data, Args... args) {
        auto instance = std::make_shared(std::forward(args)...);
        data->LayerStack.insert(data->LayerStack.begin(), std::move(instance));
    }

    Application::Application() {
        m_Data = new ApplicationData;
        m_Data->LastTimestamp = std::chrono::high_resolution_clock::now();
        m_Data->AppView = std::unique_ptr<View>(View::Create("風神", { 1600, 900 }));

        // todo: push layers
    }

    Application::~Application() { delete m_Data; }

    void Application::ProcessEvent(Event& event) {
        ZoneScoped;

        // todo: process through special utils

        for (const auto& layer : m_Data->LayerStack) {
            event.SetProcessed(layer->ProcessEvent(event));
        }
    }

    View& Application::GetView() { return *m_Data->AppView; }
    const View& Application::GetView() const { return *m_Data->AppView; }

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