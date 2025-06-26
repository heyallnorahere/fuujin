#pragma once
#include "fuujin/renderer/DeviceBuffer.h"

#include "fuujin/platform/vulkan/Vulkan.h"
#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    class VulkanBuffer : public DeviceBuffer {
    public:
        VulkanBuffer(Ref<VulkanDevice> device, const VmaAllocator& allocator, const Spec& spec);
        virtual ~VulkanBuffer() override;

        virtual const Spec& GetSpec() const override { return m_Spec; }

        VkBuffer Get() const { return m_Buffer; }

        VkDescriptorType GetDescriptorType() const;

        virtual Buffer RT_Map() override;
        virtual void RT_Unmap() override;

        virtual void RT_CopyToBuffer(CommandList& cmdlist, const Ref<DeviceBuffer>& destination,
                                     size_t size = 0, size_t srcOffset = 0,
                                     size_t dstOffset = 0) const override;

        virtual void RT_CopyToImage(CommandList& cmdlist, const Ref<DeviceImage>& destination,
                                    size_t offset = 0, const ImageCopy& copy = {}) const override;

    private:
        void RT_Allocate(VmaAllocator allocator);

        Ref<VulkanDevice> m_Device;
        VmaAllocator m_Allocator;

        VmaAllocation m_Allocation;
        VkBuffer m_Buffer;

        Spec m_Spec;
    };
} // namespace fuujin