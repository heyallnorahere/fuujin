#pragma once

namespace fuujin {
    class Event;
    class Renderer {
    public:
        Renderer() = delete;

        static void Init();
        static void Shutdown();

        static void ProcessEvent(Event& event);

        // submits a job to the render thread
        // all jobs will be executed in order submitted
        // if a job is submitted inside the render thread, the job will be executed immediately
        static void Submit(const std::function<void()>& callback,
                           const std::optional<std::string>& label = {});

        // waits for all jobs on the render thread to finish
        // returns if the jobs actually finished
        static bool Wait(std::optional<std::chrono::milliseconds> timeout = {});

        // checks if the thread this is called from is the render thread
        static bool IsRenderThread();
    };
} // namespace fuujin