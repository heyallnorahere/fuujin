#include "fuujinpch.h"
#include "fuujin/Application.h"

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
        while (true) {
            s_App->Update();
        }

        s_App.reset();
        return 0;
    }

    struct ApplicationData {
        // todo: data
    };

    Application::Application() {
        m_Data = new ApplicationData;
    }

    Application::~Application() {
        delete m_Data;
    }

    void Application::Update() {
        ZoneScoped;
        // todo: stuff
    }
}