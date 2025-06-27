#include "fuujinpch.h"
#include "fuujin/renderer/Renderer.h"

#include "fuujin/renderer/ShaderLibrary.h"

#include "fuujin/core/Events.h"

#include <thread>
#include <queue>
#include <mutex>
#include <stack>

#include <spdlog/stopwatch.h>
#include <spdlog/fmt/chrono.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(size) ::fuujin::allocate(size)
#define STBI_REALLOC(block, size) ::fuujin::reallocate(block, size)
#define STBI_FREE(block) ::fuujin::freemem(block)
#include <stb_image.h>

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
        RendererAPI* API;

        Ref<Sampler> DefaultSampler;

        uint32_t FrameCount;
        std::optional<uint32_t> CurrentFrame;

        std::stack<ActiveRenderTarget> Targets;
    };

    static std::unique_ptr<RendererData> s_Data;
    static void RenderThread() {
        tracy::SetThreadName("Render thread");
        s_Data->RenderThread.ID = std::this_thread::get_id();

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

    static void LogGraphicsContext() {
        auto device = s_Data->Context->GetDevice();

        GraphicsDevice::Properties properties;
        device->GetProperties(properties);

        FUUJIN_DEBUG("Graphics device:");
        FUUJIN_DEBUG("\tName: {}", properties.Name.c_str());
        FUUJIN_DEBUG("\tVendor: {}", properties.Vendor.c_str());

        FUUJIN_DEBUG("\tDriver version: {}.{}.{}", properties.DriverVersion.Major,
                     properties.DriverVersion.Minor, properties.DriverVersion.Patch);

        std::string type;
        switch (properties.Type) {
        case GraphicsDeviceType::Discrete:
            type = "Discrete";
            break;
        case GraphicsDeviceType::CPU:
            type = "CPU";
            break;
        case GraphicsDeviceType::Integrated:
            type = "Integrated";
            break;
        case GraphicsDeviceType::Other:
        default:
            type = "Other";
            break;
        }

        FUUJIN_DEBUG("\tType: {}", type.c_str());
        FUUJIN_DEBUG("\tAPI: {}", properties.API.c_str());

        FUUJIN_DEBUG("\tAPI version: {}.{}.{}", properties.APIVersion.Major,
                     properties.APIVersion.Minor, properties.APIVersion.Patch);
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
        Renderer::Submit([]() { LogGraphicsContext(); });

        uint32_t frameCount = 3;
        auto swapchain = s_Data->Context->GetSwapchain();

        if (swapchain) {
            frameCount = (uint32_t)swapchain->GetSyncFrameCount();
        }

        s_Data->API = s_Data->Context->CreateRendererAPI(frameCount);
        s_Data->FrameCount = frameCount;

        Sampler::Spec spec{}; // default (linear, repeat)
        s_Data->DefaultSampler = s_Data->Context->CreateSampler(spec);

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

        delete s_Data->API;
        s_Data->DefaultSampler.Reset();
        s_Data->Library.reset();
        s_Data->GraphicsQueue.Reset();
        s_Data->Context.Reset();

        Wait();
        s_Data.reset();
    }

    Ref<GraphicsContext> Renderer::GetContext() {
        ZoneScoped;
        if (!s_Data) {
            return nullptr;
        }

        return s_Data->Context;
    }

    Ref<CommandQueue> Renderer::GetGraphicsQueue() {
        ZoneScoped;
        if (!s_Data) {
            return nullptr;
        }

        return s_Data->GraphicsQueue;
    }

    ShaderLibrary& Renderer::GetShaderLibrary() {
        ZoneScoped;
        if (!s_Data) {
            throw std::runtime_error("Renderer has not been initialized!");
        }

        return *s_Data->Library;
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

    Ref<RendererAllocation> Renderer::CreateAllocation(const Ref<Shader>& shader) {
        ZoneScoped;
        return s_Data->API->CreateAllocation(shader);
    }

    struct TextureLoadData {
        Ref<DeviceBuffer> StagingBuffer;
        Ref<Texture> Texture;
    };

    Ref<Texture> Renderer::LoadTexture(const fs::path& path, const Ref<Sampler>& sampler) {
        ZoneScoped;
        stbi_set_flip_vertically_on_load(true);

        int width, height, channels;
        auto pathString = path.string();
        auto dataRaw = stbi_load(pathString.c_str(), &width, &height, &channels, 4);

        if (dataRaw == nullptr) {
            return nullptr;
        }

        DeviceBuffer::Spec stagingSpec;
        stagingSpec.QueueOwnership = { QueueType::Transfer };
        stagingSpec.Size = (size_t)width * height * channels;
        stagingSpec.Usage = DeviceBuffer::Usage::Staging;

        Texture::Spec textureSpec;
        textureSpec.Sampler = sampler.IsPresent() ? sampler : s_Data->DefaultSampler;

        textureSpec.Width = (uint32_t)width;
        textureSpec.Height = (uint32_t)height;
        textureSpec.Depth = 1;
        textureSpec.MipLevels = 1; // todo: mipmaps
        textureSpec.Format = Texture::Format::RGBA8;
        textureSpec.Type = Texture::Type::_2D;

        auto stagingBuffer = s_Data->Context->CreateBuffer(stagingSpec);
        auto texture = s_Data->Context->CreateTexture(textureSpec);

        auto loadData = new TextureLoadData;
        loadData->StagingBuffer = stagingBuffer;
        loadData->Texture = texture;

        Renderer::Submit([=]() {
            size_t dataSize = loadData->StagingBuffer->GetSpec().Size;
            auto imageData = Buffer::Wrapper(dataRaw, dataSize);

            auto mapped = loadData->StagingBuffer->RT_Map();
            Buffer::Copy(imageData, mapped);
            loadData->StagingBuffer->RT_Unmap();

            auto transferQueue = s_Data->Context->GetQueue(QueueType::Transfer);
            auto& cmdlist = transferQueue->RT_Get();
            cmdlist.RT_Begin();

            auto image = loadData->Texture->GetImage();
            loadData->StagingBuffer->RT_CopyToImage(cmdlist, image);
            cmdlist.AddDependency(loadData->StagingBuffer);

            if (loadData->Texture->GetSpec().MipLevels > 1) {
                // todo: generate mipmaps
            }

            cmdlist.RT_End();
            transferQueue->RT_Submit(cmdlist);

            stbi_image_free(dataRaw);
            delete loadData;
        });

        return texture;
    }

    void Renderer::Submit(const std::function<void()>& callback,
                          const std::optional<std::string>& label) {
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

    static void RT_NewFrame() {
        ZoneScoped;

        if (!s_Data->Targets.empty()) {
            throw std::runtime_error(
                "Must close all render targets before initiating a new frame!");
        }

        uint32_t frame;
        if (s_Data->CurrentFrame.has_value()) {
            frame = s_Data->CurrentFrame.value();

            frame++;
            frame %= s_Data->FrameCount;

            s_Data->CurrentFrame = frame;
        } else {
            s_Data->CurrentFrame = frame = 0;
        }

        s_Data->API->RT_NewFrame(frame);
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
        s_Data->API->RT_SetViewport(*target.CmdList, target.Target);
    }

    static void RT_EndRenderTarget() {
        ZoneScoped;

        auto& target = s_Data->Targets.top();
        target.Target->RT_EndRender(*target.CmdList);

        if (target.Target->GetType() == RenderTargetType::Swapchain) {
            s_Data->API->RT_PrePresent(*target.CmdList);
        }

        auto fence = target.Target->GetCurrentFence();
        fence->Reset();

        target.CmdList->RT_End();
        s_Data->GraphicsQueue->RT_Submit(*target.CmdList, fence);

        target.CmdList = nullptr;
        target.Target->RT_EndFrame();
    }

    static void RT_PushRenderTarget(Ref<RenderTarget> target) {
        ZoneScoped;

        auto& newTarget = s_Data->Targets.emplace();
        newTarget.Target = target;
        newTarget.CmdList = nullptr;

        if (target->GetType() == RenderTargetType::Swapchain) {
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

    void Renderer::NewFrame() {
        ZoneScoped;

        Renderer::Submit([]() { RT_NewFrame(); }, "New frame");
    }

    void Renderer::PushRenderTarget(Ref<RenderTarget> target) {
        ZoneScoped;

        auto targetRaw = target.Raw();
        Renderer::Submit([=]() { RT_PushRenderTarget(targetRaw); }, "Push render target");
    }

    void Renderer::PopRenderTarget() {
        ZoneScoped;

        Renderer::Submit([]() { RT_PopRenderTarget(); }, "Pop render target");
    }

    void Renderer::RenderIndexed(const IndexedRenderCall& data) {
        ZoneScoped;

        auto copy = new IndexedRenderCall;
        *copy = data;

        Renderer::Submit([copy]() {
            RT_BeginRenderTarget();

            const auto& target = s_Data->Targets.top();
            s_Data->API->RT_RenderIndexed(*target.CmdList, *copy);

            delete copy;
        });
    }
}; // namespace fuujin