#pragma once
#include "fuujin/core/View.h"

namespace fuujin {
    class Event;
    class Layer;

    struct ApplicationData;
    class Application {
    public:
        static Application& Get();
        static int Run(const std::function<void()>& initialization);

        template <typename _Ty, typename... Args>
        static void PushLayer(Args&&... args) {
            auto layer = new _Ty(std::forward<Args>(args)...);

            auto& app = Get();
            app.PushLayer(layer);
        }

        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void ProcessEvent(Event& event);

        Ref<View> GetView() const;

    private:
        Application();

        void PushLayer(Layer* layer);

        void Loop();
        void Update(Duration delta);

        ApplicationData* m_Data;
    };
} // namespace fuujin