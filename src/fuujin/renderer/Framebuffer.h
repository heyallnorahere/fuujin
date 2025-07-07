#pragma once
#include "fuujin/core/Ref.h"
#include "fuujin/renderer/CommandQueue.h"

namespace fuujin {
    enum class RenderTargetType {
        Swapchain,
        Framebuffer
    };

    class RenderTarget : public RefCounted {
    public:
        virtual void RT_BeginRender(CommandList& cmdList) = 0;
        virtual void RT_EndRender(CommandList& cmdList) = 0;

        virtual void RT_EndFrame() {};
        
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual RenderTargetType GetType() const = 0;

        virtual uint32_t GetID() const = 0;

        virtual Ref<Fence> GetCurrentFence() const { return {}; }
    };

    class Framebuffer : public RenderTarget {
    public:
        virtual void AddImageDependency(CommandList& cmdList) = 0;

        virtual RenderTargetType GetType() const override { return RenderTargetType::Framebuffer; }
    };
} // namespace fuujin