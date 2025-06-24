#pragma once
#include "fuujin/renderer/CommandQueue.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    class VulkanFence : public Fence {
    public:
        VulkanFence(Ref<VulkanDevice> device, bool signaled = false);
        virtual ~VulkanFence() override;

        virtual void Wait(uint64_t timeout) const override;
        virtual void Reset() override;

        virtual bool RT_IsReady() const override;

        VkFence Get() const { return m_Fence; }

    private:
        void RT_Create(bool signaled);

        Ref<VulkanDevice> m_Device;
        VkFence m_Fence;
    };

    class VulkanSemaphore : public RefCounted {
    public:
        VulkanSemaphore(Ref<VulkanDevice> device);
        virtual ~VulkanSemaphore() override;

        VkSemaphore Get() const { return m_Semaphore; }

    private:
        void RT_Create();

        Ref<VulkanDevice> m_Device;
        VkSemaphore m_Semaphore;
    };

    class VulkanCommandBuffer : public CommandList {
    public:
        VulkanCommandBuffer(Ref<VulkanDevice> device, VkCommandPool pool);
        virtual ~VulkanCommandBuffer() override;

        virtual void RT_Begin() override;
        virtual void RT_End() override;
        virtual void RT_Reset() override;

        virtual void AddSemaphore(Ref<RefCounted> semaphore, SemaphoreUsage usage) override;
        virtual void AddDependency(Ref<RefCounted> object) override;

        void AddWaitSemaphore(Ref<VulkanSemaphore> semaphore, VkPipelineStageFlags wait);
        bool GetSemaphores(SemaphoreUsage usage,
                           std::vector<Ref<VulkanSemaphore>>& semaphores) const;

        VkCommandBuffer Get() const { return m_Buffer; }

        const std::unordered_map<size_t, VkPipelineStageFlags> GetWaitStages() const {
            return m_WaitStages;
        }

    private:
        void RT_Alloc();

        Ref<VulkanDevice> m_Device;
        std::unordered_map<SemaphoreUsage, std::vector<Ref<VulkanSemaphore>>> m_Semaphores;
        std::unordered_map<size_t, VkPipelineStageFlags> m_WaitStages;
        std::vector<Ref<RefCounted>> m_Dependencies;

        VkCommandPool m_Pool;
        VkCommandBuffer m_Buffer;
    };

    class VulkanCommandQueue : public CommandQueue {
    public:
        VulkanCommandQueue(Ref<VulkanDevice> device, QueueType type);
        virtual ~VulkanCommandQueue() override;

        virtual QueueType GetType() const override { return m_Type; }

        virtual CommandList& RT_Get() override;
        virtual void RT_Submit(CommandList& cmdList, Ref<Fence> fence = {}) override;

        virtual void Wait() const override;
        virtual void Clear() override;

    private:
        struct StoredCommandBuffer {
            VulkanCommandBuffer* Buffer;
            Ref<VulkanFence> BufferWait;
            bool FenceOwned;
        };

        void RT_Create();
        void RT_GetQueue(uint32_t family);
        void RT_CreatePool(uint32_t family);

        QueueType m_Type;
        Ref<VulkanDevice> m_Device;

        VkQueue m_Queue;
        VkCommandPool m_Pool;

        std::queue<Ref<VulkanFence>> m_AvailableFences;
        std::queue<StoredCommandBuffer> m_StoredBuffers;
        std::mutex m_Mutex;
    };
} // namespace fuujin