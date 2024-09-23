#include "fuujinpch.h"
#include "fuujin/renderer/Renderer.h"

#include <thread>
#include <queue>
#include <mutex>

#include <spdlog/stopwatch.h>
#include <spdlog/fmt/chrono.h>

using namespace std::chrono_literals;

namespace fuujin {
    struct RendererData {
        struct {
            std::thread Thread;
            std::mutex Mutex;
            std::queue<std::function<void()>> Queue;
            bool Running;
        } RenderThread;
    };

    static std::unique_ptr<RendererData> s_Data;
    static void RenderThread() {
        tracy::SetThreadName("Fuujin render thread");

        ZoneScoped;
        FUUJIN_DEBUG("Render thread starting...");

        while (true) {
            std::optional<std::function<void()>> callback;

            {
                std::lock_guard lock(s_Data->RenderThread.Mutex);

                auto& queue = s_Data->RenderThread.Queue;
                if (!queue.empty()) {
                    callback = queue.front();
                }
            }

            if (callback.has_value()) {
                callback.value()();

                std::lock_guard lock(s_Data->RenderThread.Mutex);
                s_Data->RenderThread.Queue.pop();
            } else {
                std::this_thread::sleep_for(1ms);
            }

            if (!s_Data->RenderThread.Running) {
                break;
            }
        }
    }

    void Renderer::Init() {
        ZoneScoped;

        if (s_Data) {
            FUUJIN_WARN("Renderer already initialized - skipping second initialization");
            return;
        }

        s_Data = std::make_unique<RendererData>();
        s_Data->RenderThread.Running = true;
        s_Data->RenderThread.Thread = std::thread(RenderThread);
        s_Data->RenderThread.Thread.detach();

        // todo: initialize
    }

    void Renderer::Shutdown() {
        ZoneScoped;
        if (!s_Data) {
            FUUJIN_WARN("Renderer not initialized - skipping");
            return;
        }

        // todo: shut down

        Wait();
        s_Data.reset();
    }

    void Renderer::Submit(const std::function<void()>& callback) {
        ZoneScoped;

        std::lock_guard(s_Data->RenderThread.Mutex);
        s_Data->RenderThread.Queue.push(callback);
    }

    void Renderer::Wait() {
        ZoneScoped;

        FUUJIN_DEBUG("Waiting for render queue to finish...");
        spdlog::stopwatch timer;

        while (true) {
            {
                std::lock_guard lock(s_Data->RenderThread.Mutex);
                if (s_Data->RenderThread.Queue.empty()) {
                    break;
                }
            }

            std::this_thread::sleep_for(1ms);
        }

        FUUJIN_DEBUG("Waited {} to clear render queue",
                     std::chrono::duration_cast<std::chrono::milliseconds>(timer.elapsed()));
    }
}; // namespace fuujin