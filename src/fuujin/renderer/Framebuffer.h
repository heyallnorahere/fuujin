#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/CommandQueue.h"
#include "fuujin/renderer/Texture.h"

namespace fuujin {
    enum class RenderTargetType {
        Swapchain,
        Framebuffer
    };

    class CommandList;
    class RenderTarget : public RefCounted {
    public:
        virtual void RT_BeginRender(CommandList& cmdList, const glm::vec4& clearColor) = 0;
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
        enum class AttachmentType {
            Color,
            Depth
        };

        struct AttachmentSpec {
            Texture::Type ImageType;
            Texture::Format Format;
            AttachmentType Type;
        };

        struct Spec {
            uint32_t Width, Height;
            std::vector<AttachmentSpec> Attachments;

            std::unordered_map<size_t, size_t> ResolveAttachments;
            std::set<Texture::Feature> AttachmentFeatures;

            uint32_t Layers = 1;
        };

        virtual const Spec& GetSpec() const = 0;

        virtual Ref<Texture> GetTexture(size_t index) const = 0;

        virtual Ref<Framebuffer> Recreate(uint32_t width, uint32_t height) const = 0;

        virtual RenderTargetType GetType() const override { return RenderTargetType::Framebuffer; }
    };
} // namespace fuujin
