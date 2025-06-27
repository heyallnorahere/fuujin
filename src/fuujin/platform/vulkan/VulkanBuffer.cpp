#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanBuffer.h"

#include "fuujin/platform/vulkan/VulkanCommandQueue.h"
#include "fuujin/platform/vulkan/VulkanContext.h"
#include "fuujin/platform/vulkan/VulkanImage.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    VulkanBuffer::VulkanBuffer(Ref<VulkanDevice> device, const VmaAllocator& allocator,
                               const Spec& spec) {
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

    void VulkanBuffer::RT_CopyToBuffer(CommandList& cmdlist, const Ref<DeviceBuffer>& destination,
                                       size_t size, size_t srcOffset, size_t dstOffset) const {
        ZoneScoped;

        auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();
        auto context = Renderer::GetContext().As<VulkanContext>();
        TracyVkZone(context->GetTracyContext(), cmdBuffer, "RT_CopyToBuffer");

        VkBufferCopy region{};
        region.size = size > 0 ? size : m_Spec.Size;
        region.srcOffset = srcOffset;
        region.dstOffset = dstOffset;

        auto dst = destination.As<VulkanBuffer>();
        vkCmdCopyBuffer(cmdBuffer, m_Buffer, dst->Get(), 1, &region);
    }

    void VulkanBuffer::RT_CopyToImage(CommandList& cmdlist, const Ref<DeviceImage>& destination,
                                      size_t offset, const ImageCopy& copy) const {
        ZoneScoped;

        auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();
        auto context = Renderer::GetContext().As<VulkanContext>();
        TracyVkZone(context->GetTracyContext(), cmdBuffer, "RT_CopyToImage");

        auto image = destination.As<VulkanImage>();
        const auto& spec = image->GetSpec();

        VkBufferImageCopy region{};
        region.bufferOffset = offset;
        region.imageOffset.x = copy.X;
        region.imageOffset.y = copy.Y;
        region.imageOffset.z = copy.Z;
        region.imageExtent.width = copy.Width > 0 ? copy.Width : spec.Extent.width - copy.X;
        region.imageExtent.height = copy.Height > 0 ? copy.Height : spec.Extent.height - copy.Y;
        region.imageExtent.depth = copy.Depth > 0 ? copy.Depth : spec.Extent.depth - copy.Z;
        region.imageSubresource.aspectMask = spec.AspectFlags;
        region.imageSubresource.mipLevel = copy.MipLevel;
        region.imageSubresource.baseArrayLayer = copy.ArrayOffset;
        region.imageSubresource.layerCount =
            copy.ArraySize > 0 ? copy.ArraySize : spec.ArrayLayers - copy.ArrayOffset;

        static constexpr VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        ImageTransition transition(cmdBuffer, image, layout, VK_PIPELINE_STAGE_TRANSFER_BIT);

        vkCmdCopyBufferToImage(cmdBuffer, m_Buffer, image->GetImage(), layout, 1, &region);
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