#include "fuujinpch.h"
#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/GraphicsContext.h"

#include <thread>
#include <queue>
#include <mutex>

#include <spdlog/stopwatch.h>
#include <spdlog/fmt/chrono.h>

using namespace std::chrono_literals;

namespace fuujin {
    struct QueueCallback {
        std::function<void()> Callback;
        std::string Label;
    };

    struct RendererData {
        struct {
            std::thread Thread;
            std::mutex Mutex;
            std::queue<QueueCallback> Queue;
        } RenderThread;

        Ref<GraphicsContext> Context;
    };

    static std::unique_ptr<RendererData> s_Data;
    static void RenderThread() {
        tracy::SetThreadName("Fuujin render thread");

        ZoneScoped;
        FUUJIN_DEBUG("Render thread starting...");

        while (s_Data) {
            bool doSleep = false;

            {
                std::lock_guard lock(s_Data->RenderThread.Mutex);
                auto& queue = s_Data->RenderThread.Queue;

                if (!queue.empty()) {
                    auto callback = queue.front();
                    
                    FUUJIN_TRACE("Render thread: {}", callback.Label.c_str());
                    callback.Callback();

                    queue.pop();
                } else {
                    doSleep = true;
                }
            }

            if (doSleep) {
                std::this_thread::sleep_for(1ms);
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
        s_Data->RenderThread.Thread = std::thread(RenderThread);
        s_Data->RenderThread.Thread.detach();

        s_Data->Context = GraphicsContext::Get();
    }

    void Renderer::Shutdown() {
        ZoneScoped;
        if (!s_Data) {
            FUUJIN_WARN("Renderer not initialized - skipping");
            return;
        }

        s_Data->Context.Reset();

        Wait();
        s_Data.reset();
    }

    void Renderer::Submit(const std::function<void()>& callback, const std::optional<std::string>& label) {
        ZoneScoped;

        QueueCallback data;
        data.Callback = callback;
        data.Label = label.value_or("<unnamed render task>");

        std::lock_guard lock(s_Data->RenderThread.Mutex);
        s_Data->RenderThread.Queue.push(data);
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