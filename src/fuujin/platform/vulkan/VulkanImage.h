#pragma once
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    class VulkanImage : public RefCounted {
    public:
        struct VulkanSpec {
            VkExtent3D Extent;
            VkFormat Format;
            VkImageType Type;
            VkImageViewType ViewType;
            VkImageUsageFlags Usage;

            uint32_t MipLevels = 1;
            uint32_t ArrayLayers = 1;
            VkImage ExistingImage = VK_NULL_HANDLE;
            VkImageCreateFlags Flags = 0;
            VkImageViewCreateFlags ViewFlags = 0;
            VkImageAspectFlags AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            std::unordered_set<QueueType> QueueOwnership;
            
            VmaAllocator Allocator = VK_NULL_HANDLE;
        };

        VulkanImage(const Ref<VulkanDevice>& device, const VulkanSpec& spec);
        virtual ~VulkanImage() override;

        const VkExtent3D& GetExtent() const { return m_Spec.Extent; }

        VkImage GetImage() const { return m_Image; }
        VkImageView GetView() const { return m_View; }
        const VulkanSpec& GetSpec() const { return m_Spec; }

    private:
        void RT_CreateImage();
        void RT_CreateView();

        Ref<VulkanDevice> m_Device;

        VulkanSpec m_Spec;

        VkImage m_Image;
        VkImageView m_View;

        VmaAllocation m_Allocation = VK_NULL_HANDLE;
    };
} // namespace fuujin