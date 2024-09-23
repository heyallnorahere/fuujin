#include "fuujin.h"

#include <iostream>

class TestLayer : public fuujin::Layer {
public:
    TestLayer(const std::string& name) : m_Name(name) {}

    virtual bool ProcessEvent(fuujin::Event& event) override {
        fuujin::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<fuujin::FramebufferResizedEvent>(
            fuujin::EventType::FramebufferResized,
            std::bind(&TestLayer::ViewResized, this, std::placeholders::_1));
        
        return dispatcher;
    }

private:
    bool ViewResized(const fuujin::FramebufferResizedEvent& event) {
        std::cout << "Layer " << m_Name << " received resize event:" << std::endl;
        std::cout << "\tWidth: " << event.GetSize().Width << std::endl;
        std::cout << "\tHeight: " << event.GetSize().Height << std::endl;

        return false;
    }

    std::string m_Name;
};

int main(int argc, const char** argv) {
    ZoneScoped;
    return fuujin::Application::Run(
        []() { fuujin::Application::PushLayer<TestLayer>(std::string("Test Layer")); });
}