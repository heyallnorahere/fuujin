#pragma once

namespace fuujin {
    class Renderer {
    public:
        Renderer() = delete;

        static void Init();
        static void Shutdown();

        static void Submit(const std::function<void()>& callback,
                           const std::optional<std::string>& label = {});
                           
        static void Wait();
    };
} // namespace fuujin