#pragma once
#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanInstance.h"
#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanSwapchain.h"

namespace fuujin {
    struct ContextData;

    class VulkanContext : public GraphicsContext {
    public:
        static Ref<VulkanContext> Get(const std::optional<std::string>& deviceName = {});
        static VkAllocationCallbacks& GetAllocCallbacks();

        virtual ~VulkanContext() override;

        Ref<VulkanInstance> GetInstance() const;
        Ref<VulkanDevice> GetVulkanDevice(const std::optional<std::string>& deviceName = {}) const;

        TracyVkCtx GetTracyContext() const;

        virtual Ref<GraphicsDevice> GetDevice() const override;
        virtual Ref<Swapchain> GetSwapchain() const override;
        virtual Ref<CommandQueue> GetQueue(QueueType type) const override;

        virtual Ref<Fence> CreateFence(bool signaled) const override;
        virtual Ref<RefCounted> CreateSemaphore() const override;

        virtual Ref<Shader> LoadShader(const Shader::Code& code) const override;
        virtual Ref<Pipeline> CreatePipeline(const Pipeline::Spec& spec) const override;

        virtual Ref<DeviceBuffer> CreateBuffer(const DeviceBuffer::Spec& spec) const override;
        virtual Ref<Sampler> CreateSampler(const Sampler::Spec& spec) const override;
        virtual Ref<Texture> CreateTexture(const Texture::Spec& spec) const override;

        virtual RendererAPI* CreateRendererAPI(uint32_t frames) const override;

    private:
        VulkanContext(const std::optional<std::string>& deviceName);

        void RT_LoadInstance() const;
        void RT_LoadDevice() const;

        void RT_QueryPresentQueue();
        void RT_CreateAllocator();
        void RT_CreateTracyContext();

        void RT_CreateDebugMessenger();
        void RT_EnumerateDevices(const std::optional<std::string>& deviceName);

        ContextData* m_Data;
    };
} // namespace fuujin