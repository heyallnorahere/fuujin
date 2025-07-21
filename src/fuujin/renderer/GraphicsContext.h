#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/renderer/CommandQueue.h"
#include "fuujin/renderer/Swapchain.h"
#include "fuujin/renderer/Shader.h"
#include "fuujin/renderer/Pipeline.h"
#include "fuujin/renderer/DeviceBuffer.h"
#include "fuujin/renderer/Texture.h"

namespace fuujin {
    class RendererAPI;
    class GraphicsContext : public RefCounted {
    public:
        static Ref<GraphicsContext> Get(const std::optional<std::string>& deviceName = {});

        virtual Ref<GraphicsDevice> GetDevice() const = 0;
        virtual Ref<Swapchain> GetSwapchain() const = 0;
        virtual Ref<CommandQueue> GetQueue(QueueType type) const = 0;

        virtual Ref<Fence> CreateFence(bool signaled = false) const = 0;
        virtual Ref<RefCounted> CreateSemaphore() const = 0;

        virtual Ref<Shader> LoadShader(const Shader::Code& code) const = 0;
        virtual Ref<Pipeline> CreatePipeline(const Pipeline::Spec& spec) const = 0;

        virtual Ref<DeviceBuffer> CreateBuffer(const DeviceBuffer::Spec& spec) const = 0;
        virtual Ref<Sampler> CreateSampler(const Sampler::Spec& spec) const = 0;
        virtual Ref<Texture> CreateTexture(const Texture::Spec& spec) const = 0;

        virtual Ref<Swapchain> CreateSwapchain(const Ref<View>& view) const = 0;

        virtual RendererAPI* CreateRendererAPI(uint32_t frames) const = 0;

        virtual bool AreDiscreteQueues(const std::unordered_set<QueueType>& types) const = 0;
    };
} // namespace fuujin
