#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanCommandQueue.h"

#include "fuujin/renderer/Renderer.h"

#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    static void RT_WaitForFence(VkDevice device, VkFence fence, uint64_t timeout) {
        ZoneScoped;

        vkWaitForFences(device, 1, &fence, VK_TRUE, timeout);
    }

    VulkanFence::VulkanFence(Ref<VulkanDevice> device, bool signaled) {
        ZoneScoped;
        m_Device = device;
        m_Fence = VK_NULL_HANDLE;

        Renderer::Submit([this, signaled]() { RT_Create(signaled); }, "Create fence");
    }

    VulkanFence::~VulkanFence() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto fence = m_Fence;

        Renderer::Submit(
            [device, fence]() {
                vkDestroyFence(device, fence, &VulkanContext::GetAllocCallbacks());
            },
            "Destroy fence");
    }

    void VulkanFence::Wait(uint64_t timeout) const {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto fence = m_Fence;

        Renderer::Submit([=]() { RT_WaitForFence(device, fence, timeout); }, "Wait for fence");
    }

    void VulkanFence::Reset() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto fence = m_Fence;

        Renderer::Submit([=]() { vkResetFences(device, 1, &fence); }, "Reset fence");
    }

    bool VulkanFence::RT_IsReady() const {
        ZoneScoped;

        VkResult result = vkGetFenceStatus(m_Device->GetDevice(), m_Fence);
        return result == VK_SUCCESS;
    }

    void VulkanFence::RT_Create(bool signaled) {
        ZoneScoped;

        VkFenceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (signaled) {
            createInfo.flags |= VK_FENCE_CREATE_SIGNALED_BIT;
        }

        if (vkCreateFence(m_Device->GetDevice(), &createInfo, &VulkanContext::GetAllocCallbacks(),
                          &m_Fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence!");
        }
    }

    VulkanSemaphore::VulkanSemaphore(Ref<VulkanDevice> device) {
        ZoneScoped;

        m_Device = device;
        m_Semaphore = VK_NULL_HANDLE;

        Renderer::Submit([this]() { RT_Create(); }, "Create semaphore");
    }

    VulkanSemaphore::~VulkanSemaphore() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto semaphore = m_Semaphore;

        Renderer::Submit(
            [device, semaphore]() {
                vkDestroySemaphore(device, semaphore, &VulkanContext::GetAllocCallbacks());
            },
            "Destroy semaphore");
    }

    void VulkanSemaphore::RT_Create() {
        ZoneScoped;

        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(m_Device->GetDevice(), &createInfo,
                              &VulkanContext::GetAllocCallbacks(), &m_Semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore!");
        }
    }

    VulkanCommandBuffer::VulkanCommandBuffer(Ref<VulkanDevice> device, VkCommandPool pool) {
        ZoneScoped;

        m_Device = device;
        m_Pool = pool;
        m_Buffer = VK_NULL_HANDLE;

        Renderer::Submit([this]() { RT_Alloc(); }, "Allocate command buffer");
    }

    VulkanCommandBuffer::~VulkanCommandBuffer() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto pool = m_Pool;
        auto buffer = m_Buffer;

        Renderer::Submit([=]() { vkFreeCommandBuffers(device, pool, 1, &buffer); },
                         "Free command buffer");
    }

    void VulkanCommandBuffer::RT_Begin() {
        ZoneScoped;

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(m_Buffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording of command buffer!");
        }
    }

    void VulkanCommandBuffer::RT_End() {
        ZoneScoped;

        if (vkEndCommandBuffer(m_Buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end recording of command buffer!");
        }
    }

    void VulkanCommandBuffer::RT_Reset() {
        ZoneScoped;

        vkResetCommandBuffer(m_Buffer, 0);
        m_Semaphores.clear();
        m_WaitStages.clear();
        m_SemaphoreDependencyMap.clear();
        m_Dependencies.clear();
    }

    void VulkanCommandBuffer::AddSemaphore(Ref<RefCounted> semaphore, SemaphoreUsage usage) {
        ZoneScoped;

        auto vkSemaphore = semaphore.As<VulkanSemaphore>();
        if (usage == SemaphoreUsage::Wait) {
            auto semaphorePtr = vkSemaphore->Get();
            if (m_SemaphoreDependencyMap.contains(semaphorePtr)) {
                return;
            }

            m_SemaphoreDependencyMap[semaphorePtr] = m_Semaphores[usage].size();
        }

        m_Semaphores[usage].push_back(vkSemaphore);
    }

    void VulkanCommandBuffer::AddDependency(Ref<RefCounted> object) {
        ZoneScoped;
        m_Dependencies.push_back(object);
    }

    void VulkanCommandBuffer::AddWaitSemaphore(Ref<VulkanSemaphore> semaphore,
                                               VkPipelineStageFlags wait) {
        ZoneScoped;

        size_t semaphoreIndex = 0;
        auto vkSemaphore = semaphore->Get();

        if (m_SemaphoreDependencyMap.contains(vkSemaphore)) {
            semaphoreIndex = m_SemaphoreDependencyMap.at(vkSemaphore);
            m_WaitStages[semaphoreIndex] |= wait;
        } else if (m_Semaphores.contains(SemaphoreUsage::Wait)) {
            semaphoreIndex = m_Semaphores.at(SemaphoreUsage::Wait).size();
            m_SemaphoreDependencyMap[vkSemaphore] = semaphoreIndex;
            m_WaitStages.insert(std::make_pair(semaphoreIndex, wait));
        }

        m_Semaphores[SemaphoreUsage::Wait].push_back(semaphore);
    }

    bool VulkanCommandBuffer::GetSemaphores(SemaphoreUsage usage,
                                            std::vector<Ref<VulkanSemaphore>>& semaphores) const {
        ZoneScoped;

        auto it = m_Semaphores.find(usage);
        if (it == m_Semaphores.end()) {
            return false;
        }

        const auto& bufferSemaphores = it->second;
        semaphores.clear();
        semaphores.insert(semaphores.end(), bufferSemaphores.begin(), bufferSemaphores.end());

        return true;
    }

    void VulkanCommandBuffer::RT_Alloc() {
        ZoneScoped;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = m_Pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        if (vkAllocateCommandBuffers(m_Device->GetDevice(), &allocInfo, &m_Buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer!");
        }
    }

    VulkanCommandQueue::VulkanCommandQueue(Ref<VulkanDevice> device, QueueType type) {
        ZoneScoped;

        m_Device = device;
        m_Type = type;

        Renderer::Submit([this]() { RT_Create(); }, "Set up command queue");
    }

    VulkanCommandQueue::~VulkanCommandQueue() {
        ZoneScoped;
        Wait();

        while (!m_StoredBuffers.empty()) {
            delete m_StoredBuffers.front().Buffer;
            m_StoredBuffers.pop();
        }

        auto device = m_Device->GetDevice();
        auto pool = m_Pool;

        Renderer::Submit(
            [=]() { vkDestroyCommandPool(device, pool, &VulkanContext::GetAllocCallbacks()); },
            "Destroy command pool");
    }

    static void CheckRenderThread() {
        if (!Renderer::IsRenderThread()) {
            throw std::runtime_error("CommandQueues must be used from the render thread!");
        }
    }

    CommandList& VulkanCommandQueue::RT_Get() {
        ZoneScoped;
        CheckRenderThread();

        std::lock_guard lock(m_Mutex);

        VulkanCommandBuffer* buffer = nullptr;
        if (!m_StoredBuffers.empty()) {
            const auto& storedBuffer = m_StoredBuffers.front();
            auto fence = storedBuffer.BufferWait;

            if (fence->RT_IsReady()) {
                if (storedBuffer.FenceOwned) {
                    fence->Reset();
                    m_AvailableFences.push(fence);
                }

                buffer = storedBuffer.Buffer;
                m_StoredBuffers.pop();

                buffer->RT_Reset();
            }
        }

        if (buffer == nullptr) {
            buffer = new VulkanCommandBuffer(m_Device, m_Pool);
        }

        return *buffer;
    }

    void VulkanCommandQueue::RT_Submit(CommandList& cmdList, Ref<Fence> fence) {
        ZoneScoped;
        CheckRenderThread();

        std::lock_guard lock(m_Mutex);

        StoredCommandBuffer stored;
        stored.Buffer = (VulkanCommandBuffer*)&cmdList;

        if (fence.IsEmpty()) {
            if (m_AvailableFences.empty()) {
                stored.BufferWait = Ref<VulkanFence>::Create(m_Device);
            } else {
                stored.BufferWait = m_AvailableFences.front();
                m_AvailableFences.pop();
            }

            stored.FenceOwned = true;
        } else {
            stored.BufferWait = fence.As<VulkanFence>();
            stored.FenceOwned = false;
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        auto cmdBuffer = stored.Buffer->Get();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        std::vector<VkPipelineStageFlags> waitStageFlags;
        std::vector<VkSemaphore> waitSemaphores, signalSemaphores;

        std::vector<Ref<VulkanSemaphore>> semaphores;
        if (stored.Buffer->GetSemaphores(SemaphoreUsage::Signal, semaphores)) {
            for (auto semaphore : semaphores) {
                signalSemaphores.push_back(semaphore->Get());
            }

            submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
            submitInfo.pSignalSemaphores = signalSemaphores.data();
        }

        if (stored.Buffer->GetSemaphores(SemaphoreUsage::Wait, semaphores)) {
            const auto& waitStages = stored.Buffer->GetWaitStages();
            for (size_t i = 0; i < semaphores.size(); i++) {
                waitSemaphores.push_back(semaphores[i]->Get());
                waitStageFlags.push_back(
                    waitStages.contains(i) ? waitStages.at(i) : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
            }

            submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
            submitInfo.pWaitSemaphores = waitSemaphores.data();
            submitInfo.pWaitDstStageMask = waitStageFlags.data();
        }

        if (vkQueueSubmit(m_Queue, 1, &submitInfo, stored.BufferWait->Get()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit command buffer to queue!");
        }

        m_StoredBuffers.push(stored);
    }

    static void RT_WaitForQueue(VkQueue queue) {
        ZoneScoped;

        if (vkQueueWaitIdle(queue) != VK_SUCCESS) {
            throw std::runtime_error("Error occurred while waiting for queue!");
        }
    }

    void VulkanCommandQueue::Wait() const {
        ZoneScoped;

        auto queue = m_Queue;
        Renderer::Submit([=]() { RT_WaitForQueue(queue); }, "Wait for queue");
    }

    void VulkanCommandQueue::Clear() {
        ZoneScoped;
        Wait();

        std::lock_guard lock(m_Mutex);

        while (!m_StoredBuffers.empty()) {
            const auto& front = m_StoredBuffers.front();

            delete front.Buffer;
            if (front.FenceOwned) {
                m_AvailableFences.push(front.BufferWait);
            }

            m_StoredBuffers.pop();
        }
    }

    void VulkanCommandQueue::RT_Create() {
        ZoneScoped;

        const auto& queues = m_Device->GetQueues();
        uint32_t family = queues.at(m_Type);

        RT_GetQueue(family);
        RT_CreatePool(family);
    }

    void VulkanCommandQueue::RT_GetQueue(uint32_t family) {
        ZoneScoped;

        vkGetDeviceQueue(m_Device->GetDevice(), family, 0, &m_Queue);
    }

    void VulkanCommandQueue::RT_CreatePool(uint32_t family) {
        ZoneScoped;

        VkCommandPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.queueFamilyIndex = family;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(m_Device->GetDevice(), &createInfo,
                                &VulkanContext::GetAllocCallbacks(), &m_Pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool for queue!");
        }
    }
} // namespace fuujin
