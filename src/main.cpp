#include "fuujin.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/Pipeline.h"
#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/renderer/ShaderLibrary.h"

using namespace std::chrono_literals;

class TestLayer : public fuujin::Layer {
public:
    TestLayer(const std::string& name) : m_Name(name) {
        FUUJIN_DEBUG("Test layer added with name: {}", name.c_str());
    }

    virtual void ProcessEvent(fuujin::Event& event) override {
        fuujin::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<fuujin::FramebufferResizedEvent>(
            fuujin::EventType::FramebufferResized,
            std::bind(&TestLayer::ViewResized, this, std::placeholders::_1));
    }

private:
    bool ViewResized(const fuujin::FramebufferResizedEvent& event) {
        FUUJIN_TRACE("Layer {} received resize event:", m_Name.c_str());
        FUUJIN_TRACE("\tWidth: {}", event.GetSize().Width);
        FUUJIN_TRACE("\tHeight: {}", event.GetSize().Height);

        return false;
    }

    std::string m_Name;
};

int RunApplication() {
    return fuujin::Application::Run(
        []() { fuujin::Application::PushLayer<TestLayer>(std::string("Test Layer")); });
}

int main(int argc, const char** argv) {
    ZoneScoped;

#ifndef FUUJIN_IS_DEBUG
    try {
        return RunApplication();
    } catch (const std::runtime_error& exc) {
        FUUJIN_CRITICAL("Error thrown: {}", exc.what());
    }
#else
    return RunApplication();
#endif
}