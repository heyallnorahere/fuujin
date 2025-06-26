#pragma once
#include "fuujin/renderer/DeviceImage.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    class VulkanImage : public DeviceImage {
    public:
        struct VulkanSpec {
            VkExtent3D Extent;
            VkFormat Format;
            VkImageType Type;
            VkImageViewType ViewType;
            VkImageUsageFlags Usage;

            VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;
            uint32_t MipLevels = 1;
            uint32_t ArrayLayers = 1;
            VkImage ExistingImage = VK_NULL_HANDLE;
            VkImageCreateFlags Flags = 0;
            VkImageViewCreateFlags ViewFlags = 0;
            VkImageAspectFlags AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
            VkImageLayout SafeLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            std::unordered_set<QueueType> QueueOwnership;

            VmaAllocator Allocator = VK_NULL_HANDLE;
        };

        static VkAccessFlags GetLayoutAccessFlags(VkImageLayout layout);

        static void RT_TransitionLayout(VkCommandBuffer cmdBuffer, VkImage image,
                                        VkImageLayout srcLayout, VkPipelineStageFlags srcStage,
                                        VkImageLayout dstLayout, VkPipelineStageFlags dstStage);

        VulkanImage(const Ref<VulkanDevice>& device, const VulkanSpec& spec);
        virtual ~VulkanImage() override;

        VkImage GetImage() const { return m_Image; }
        VkImageView GetView() const { return m_View; }
        const VulkanSpec& GetSpec() const { return m_Spec; }

        virtual uint32_t GetWidth() const override { return m_Spec.Extent.width; }
        virtual uint32_t GetHeight() const override { return m_Spec.Extent.height; }
        virtual uint32_t GetDepth() const override { return m_Spec.Extent.depth; }

        virtual uint32_t GetMipLevels() const override { return m_Spec.MipLevels; }

    private:
        void RT_CreateImage();
        void RT_CreateView();

        Ref<VulkanDevice> m_Device;

        VulkanSpec m_Spec;

        VkImage m_Image;
        VkImageView m_View;

        VmaAllocation m_Allocation = VK_NULL_HANDLE;
    };

    class ImageTransition {
    public:
        ImageTransition(VkCommandBuffer cmdBuffer, const Ref<VulkanImage>& image,
                        VkImageLayout layout, VkPipelineStageFlags stage,
                        VkPipelineStageFlags endStage = 0);

        ~ImageTransition();

        ImageTransition(const ImageTransition&) = delete;
        ImageTransition& operator=(const ImageTransition&) = delete;

    private:
        VkCommandBuffer m_Buffer;
        Ref<VulkanImage> m_Image;

        VkImageLayout m_Layout;
        VkPipelineStageFlags m_EndStage;
    };
} // namespace fuujin