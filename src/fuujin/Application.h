#pragma once

namespace fuujin {
    class View;
    class Event;
    class Layer;

    struct ApplicationData;
    class Application {
    public:
        static Application& Get();
        static int Run(int argc, const char** argv);

        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void ProcessEvent(Event& event);

        View& GetView();
        const View& GetView() const;

    private:
        Application();

        void Update();

        ApplicationData* m_Data;
    };
} // namespace fuujin