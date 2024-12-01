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
            std::thread::id ID;
            std::mutex Mutex;
            std::queue<QueueCallback> Queue;
        } RenderThread;

        Ref<GraphicsContext> Context;
    };

    static std::unique_ptr<RendererData> s_Data;
    static void RenderThread() {
        tracy::SetThreadName("Render thread");
        s_Data->RenderThread.ID = std::this_thread::get_id();

        ZoneScoped;
        FUUJIN_DEBUG("Render thread starting...");

        while (s_Data) {
            bool doSleep = false;

            {
                auto& queue = s_Data->RenderThread.Queue;
                std::optional<QueueCallback> callback;

                {
                    std::lock_guard lock(s_Data->RenderThread.Mutex);
                    if (!queue.empty()) {
                        callback = queue.front();
                    }
                }

                if (callback.has_value()) {
                    FUUJIN_TRACE("Render thread: {}", callback->Label.c_str());
                    callback->Callback();

                    {
                        std::lock_guard lock(s_Data->RenderThread.Mutex);
                        queue.pop();
                    }
                } else {
                    FUUJIN_TRACE("Render queue empty, sleeping 1ms...");
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

    void Renderer::Submit(const std::function<void()>& callback,
                          const std::optional<std::string>& label) {
        ZoneScoped;

        auto jobLabel = label.value_or("<unnamed render task>");
        if (IsRenderThread()) {
            FUUJIN_TRACE("Render thread: sub-job {}", jobLabel.c_str());
            callback();
            
            return;
        }

        QueueCallback data;
        data.Callback = callback;
        data.Label = jobLabel;

        std::lock_guard lock(s_Data->RenderThread.Mutex);
        s_Data->RenderThread.Queue.push(data);
    }

    bool Renderer::Wait(std::optional<std::chrono::milliseconds> timeout) {
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

            if (timeout.has_value() && timer.elapsed() >= timeout.value()) {
                FUUJIN_DEBUG("Waited {} to clear render queue - timed out", timer.elapsed_ms());
                return false;
            }

            std::this_thread::sleep_for(1ms);
        }

        FUUJIN_DEBUG("Waited {} to clear render queue", timer.elapsed_ms());
        return true;
    }

    bool Renderer::IsRenderThread() {
        ZoneScoped;

        return std::this_thread::get_id() == s_Data->RenderThread.ID;
    }
}; // namespace fuujin