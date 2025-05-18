#include "fuujinpch.h"
#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/renderer/ShaderLibrary.h"

#include "fuujin/core/Events.h"

#include <thread>
#include <queue>
#include <mutex>
#include <stack>

#include <spdlog/stopwatch.h>
#include <spdlog/fmt/chrono.h>

using namespace std::chrono_literals;

namespace fuujin {
    struct QueueCallback {
        std::function<void()> Callback;
        std::string Label;
    };

    struct ActiveRenderTarget {
        Ref<RenderTarget> Target;
        CommandList* CmdList;
    };

    struct RendererData {
        struct {
            std::thread Thread;
            std::thread::id ID;
            std::mutex Mutex;
            std::queue<QueueCallback> Queue;
        } RenderThread;

        Ref<GraphicsContext> Context;
        Ref<CommandQueue> GraphicsQueue;
        std::unique_ptr<ShaderLibrary> Library;

        std::stack<ActiveRenderTarget> Targets;
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
        s_Data->Library = std::make_unique<ShaderLibrary>(s_Data->Context);

        Renderer::Submit(
            []() { s_Data->GraphicsQueue = s_Data->Context->GetQueue(QueueType::Graphics); },
            "Fetch graphics queue");
    }

    void Renderer::Shutdown() {
        ZoneScoped;
        if (!s_Data) {
            FUUJIN_WARN("Renderer not initialized - skipping");
            return;
        }

        Wait();
        s_Data->Library.reset();
        s_Data->GraphicsQueue.Reset();
        s_Data->Context.Reset();

        Wait();
        s_Data.reset();
    }

    void Renderer::ProcessEvent(Event& event) {
        ZoneScoped;

        if (event.GetType() == EventType::FramebufferResized) {
            auto swapchain = s_Data->Context->GetSwapchain();
            if (swapchain.IsPresent()) {
                auto& resizeEvent = (FramebufferResizedEvent&)event;
                swapchain->RequestResize(resizeEvent.GetSize());

                resizeEvent.SetProcessed();
            }
        }
    }

    void Renderer::Submit(const std::function<void()>& callback,
                          const std::optional<std::string>& label) {
        ZoneScoped;

        auto jobLabel = label.value_or("<unnamed render task>");
        if (IsRenderThread()) {
            FUUJIN_TRACE("Render thread: inline sub-job {}", jobLabel.c_str());
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

        FUUJIN_TRACE("Waiting for render queue to finish...");
        spdlog::stopwatch timer;

        while (true) {
            {
                std::lock_guard lock(s_Data->RenderThread.Mutex);
                if (s_Data->RenderThread.Queue.empty()) {
                    break;
                }
            }

            auto elapsed = timer.elapsed_ms();
            if (timeout.has_value() && elapsed >= timeout.value()) {
                FUUJIN_WARN("Waited {} to clear render queue - timed out", elapsed);
                return false;
            }

            std::this_thread::sleep_for(1ms);
        }

        FUUJIN_TRACE("Waited {} to clear render queue", timer.elapsed_ms());
        return true;
    }

    bool Renderer::IsRenderThread() {
        ZoneScoped;

        return std::this_thread::get_id() == s_Data->RenderThread.ID;
    }

    static void RT_BeginRenderTarget() {
        ZoneScoped;

        if (s_Data->Targets.empty()) {
            throw std::runtime_error("No render targets on the stack!");
        }

        auto& target = s_Data->Targets.top();
        if (target.CmdList != nullptr) {
            return; // nothing to do
        }

        target.CmdList = &s_Data->GraphicsQueue->RT_Get();
        target.CmdList->RT_Begin();
        target.Target->RT_BeginRender(*target.CmdList);
    }

    static void RT_EndRenderTarget() {
        ZoneScoped;

        auto& target = s_Data->Targets.top();
        target.Target->RT_EndRender(*target.CmdList);
        target.CmdList->RT_End();

        auto fence = target.Target->GetCurrentFence();
        fence->Reset();
        s_Data->GraphicsQueue->RT_Submit(*target.CmdList, fence);

        target.CmdList = nullptr;
        target.Target->RT_EndFrame();
    }

    static void RT_PushRenderTarget(Ref<RenderTarget> target, bool force) {
        ZoneScoped;

        auto& newTarget = s_Data->Targets.emplace();
        newTarget.Target = target;
        newTarget.CmdList = nullptr;

        if (force) {
            RT_BeginRenderTarget();
        }
    }

    static void RT_PopRenderTarget() {
        ZoneScoped;
        if (s_Data->Targets.empty()) {
            return;
        }

        auto& target = s_Data->Targets.top();
        if (target.CmdList != nullptr) {
            RT_EndRenderTarget();
        }

        s_Data->Targets.pop();
    }

    void Renderer::PushRenderTarget(Ref<RenderTarget> target, bool force) {
        ZoneScoped;

        auto targetRaw = target.Raw();
        Renderer::Submit([=]() { RT_PushRenderTarget(targetRaw, force); }, "Push render target");
    }

    void Renderer::PopRenderTarget() {
        ZoneScoped;

        Renderer::Submit([]() { RT_PopRenderTarget(); }, "Pop render target");
    }

    ShaderLibrary& Renderer::GetShaderLibrary() { return *s_Data->Library; }
}; // namespace fuujin