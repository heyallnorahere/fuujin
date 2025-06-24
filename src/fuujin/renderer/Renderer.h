#pragma once
#include "fuujin/core/Ref.h"
#include "fuujin/renderer/Framebuffer.h"
#include "fuujin/renderer/DeviceBuffer.h"
#include "fuujin/renderer/Pipeline.h"

#include "fuujin/core/Buffer.h"

namespace fuujin {
    class Event;
    class ShaderLibrary;
    class CommandList;

    class RendererAPI {
    public:
        virtual ~RendererAPI() = default;

        virtual void RT_RenderIndexed(CommandList& cmdlist, Ref<DeviceBuffer> vertices,
                                      Ref<DeviceBuffer> indices, Ref<Pipeline> pipeline,
                                      uint32_t indexCount,
                                      const Buffer& pushConstants = Buffer()) const = 0;

        virtual void RT_SetViewport(CommandList& cmdlist, Ref<RenderTarget> target) const = 0;
    };

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

        // pushes a render target of a framebuffer
        // "force" - force begin render?
        static void PushRenderTarget(Ref<RenderTarget> target, bool force = false);

        // flushes and pops a render target from the stack
        static void PopRenderTarget();

        static void RenderIndexed(Ref<DeviceBuffer> vertices, Ref<DeviceBuffer> indices,
                                  Ref<Pipeline> pipeline, uint32_t indexCount,
                                  const Buffer& pushConstants = Buffer());

        // get the shader library attached to the current graphics context
        static ShaderLibrary& GetShaderLibrary();
    };
} // namespace fuujin