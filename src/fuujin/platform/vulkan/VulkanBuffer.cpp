#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanBuffer.h"

#include "fuujin/platform/vulkan/VulkanCommandQueue.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    VulkanBuffer::VulkanBuffer(Ref<VulkanDevice> device, const VmaAllocator& allocator, const Spec& spec) {
        ZoneScoped;

        m_Device = device;
        m_Allocator = VK_NULL_HANDLE;

        m_Buffer = VK_NULL_HANDLE;
        m_Allocation = VK_NULL_HANDLE;

        m_Spec = spec;

        Renderer::Submit([&]() { RT_Allocate(allocator); });
    }

    VulkanBuffer::~VulkanBuffer() {
        ZoneScoped;

        auto allocator = m_Allocator;
        auto buffer = m_Buffer;
        auto allocation = m_Allocation;

        Renderer::Submit([=]() { vmaDestroyBuffer(allocator, buffer, allocation); });
    }

    VkDescriptorType VulkanBuffer::GetDescriptorType() const {
        ZoneScoped;

        switch (m_Spec.Usage) {
        case Usage::Uniform:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case Usage::Storage:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        throw std::runtime_error("This buffer usage does not support binding to descriptor sets!");
    }

    Buffer VulkanBuffer::RT_Map() {
        ZoneScoped;

        void* mapped = nullptr;
        if (vmaMapMemory(m_Allocator, m_Allocation, &mapped) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map buffer memory!");
        }

        return Buffer::Wrapper(mapped, m_Spec.Size);
    }

    void VulkanBuffer::RT_Unmap() {
        ZoneScoped;
        vmaUnmapMemory(m_Allocator, m_Allocation);
    }

    void VulkanBuffer::RT_CopyTo(CommandList& cmdlist, Ref<DeviceBuffer> destination, size_t size,
                                 size_t srcOffset, size_t dstOffset) const {
        ZoneScoped;

        // todo: tracy gpu zone

        VkBufferCopy region{};
        region.size = size > 0 ? size : m_Spec.Size;
        region.srcOffset = srcOffset;
        region.dstOffset = dstOffset;

        auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();
        auto dst = destination.As<VulkanBuffer>();
        vkCmdCopyBuffer(cmdBuffer, m_Buffer, dst->Get(), 1, &region);
    }

    void VulkanBuffer::RT_Allocate(VmaAllocator allocator) {
        ZoneScoped;
        m_Allocator = allocator;

        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = m_Spec.Size;

        std::vector<uint32_t> indices;
        createInfo.sharingMode = m_Device->GetSharingMode(m_Spec.QueueOwnership, indices);

        if (indices.size() > 0) {
            createInfo.queueFamilyIndexCount = (uint32_t)indices.size();
            createInfo.pQueueFamilyIndices = indices.data();
        }

        bool deviceOnly = false;
        switch (m_Spec.Usage) {
        case Usage::Vertex:
            createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            deviceOnly = true;
            break;
        case Usage::Index:
            createInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            deviceOnly = true;
            break;
        case Usage::Uniform:
            createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        case Usage::Storage:
            createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        case Usage::Staging:
            createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        default:
            throw std::runtime_error("Invalid buffer usage!");
        }

        VmaAllocationCreateInfo allocInfo{};
        if (deviceOnly) {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        } else {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        }

        if (vmaCreateBuffer(m_Allocator, &createInfo, &allocInfo, &m_Buffer, &m_Allocation,
                            nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan buffer!");
        }
    }
} // namespace fuujin