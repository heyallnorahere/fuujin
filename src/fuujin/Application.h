#pragma once

namespace fuujin {
    class View;

    struct ApplicationData;
    class Application {
    public:
        static Application& Get();
        static int Run(int argc, const char** argv);

        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

    private:
        Application();

        void Update();

        ApplicationData* m_Data;
    };
} // namespace fuujin