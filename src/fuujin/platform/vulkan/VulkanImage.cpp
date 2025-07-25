#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanImage.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    VkAccessFlags VulkanImage::GetLayoutAccessFlags(VkImageLayout layout,
                                                    VkPipelineStageFlags stage) {
        static const VkPipelineStageFlags endStages =
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        if ((stage & ~endStages) == 0) {
            return 0;
        }

        switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return 0;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_GENERAL: // storage layout
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        default:
            throw std::runtime_error("Unimplemented!");
        }
    }

    void VulkanImage::RT_TransitionLayout(VkCommandBuffer cmdBuffer, VkImage image,
                                          VkImageLayout srcLayout, VkPipelineStageFlags srcStage,
                                          VkImageLayout dstLayout, VkPipelineStageFlags dstStage,
                                          const VkImageSubresourceRange& subresource) {
        ZoneScoped;

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.oldLayout = srcLayout;
        barrier.newLayout = dstLayout;
        barrier.srcAccessMask = GetLayoutAccessFlags(srcLayout, srcStage);
        barrier.dstAccessMask = GetLayoutAccessFlags(dstLayout, dstStage);
        barrier.subresourceRange = subresource;

        vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    VulkanImage::VulkanImage(const Ref<VulkanDevice>& device, const VulkanSpec& spec)
        : m_Device(device), m_Spec(spec), m_Image(VK_NULL_HANDLE), m_View(VK_NULL_HANDLE),
          m_Allocation(VK_NULL_HANDLE) {
        ZoneScoped;

        m_Image = spec.ExistingImage;
        if (m_Image == VK_NULL_HANDLE) {
            Renderer::Submit([&]() { RT_CreateImage(); }, "Create device image");
        }

        Renderer::Submit([&]() { RT_CreateView(); }, "Create image view");
    }

    VulkanImage::~VulkanImage() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto allocator = m_Spec.Allocator;

        auto view = m_View;
        auto image = m_Image;
        auto allocation = m_Allocation;

        Renderer::Submit(
            [device, allocator, view, image, allocation]() {
                vkDestroyImageView(device, view, &VulkanContext::GetAllocCallbacks());

                if (allocation != VK_NULL_HANDLE) {
                    vmaDestroyImage(allocator, image, allocation);
                }
            },
            "Destroy image");
    }

    Ref<VulkanSemaphore> VulkanImage::SignalUsed() {
        ZoneScoped;

        m_SignaledSemaphore = Ref<VulkanSemaphore>::Create(m_Device);
        return m_SignaledSemaphore;
    }

    Ref<VulkanSemaphore> VulkanImage::GetSignaledSemaphore() {
        ZoneScoped;

        auto semaphore = m_SignaledSemaphore;
        m_SignaledSemaphore.Reset();

        return semaphore;
    }

    void VulkanImage::RT_CreateImage() {
        ZoneScoped;

        if (m_Spec.Allocator == VK_NULL_HANDLE) {
            throw std::runtime_error("Allocator not passed!");
        }

        VkImageCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        createInfo.flags = m_Spec.Flags;
        createInfo.format = m_Spec.Format;
        createInfo.imageType = m_Spec.Type;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        createInfo.mipLevels = m_Spec.MipLevels;
        createInfo.arrayLayers = m_Spec.ArrayLayers;
        createInfo.extent = m_Spec.Extent;
        createInfo.usage = m_Spec.Usage;
        createInfo.samples = m_Spec.Samples;
        createInfo.tiling = m_Spec.Tiling;

        std::vector<uint32_t> indices;
        createInfo.sharingMode = m_Device->GetSharingMode(m_Spec.QueueOwnership, indices);

        if (indices.size() > 0) {
            createInfo.queueFamilyIndexCount = (uint32_t)indices.size();
            createInfo.pQueueFamilyIndices = indices.data();
        }

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (vmaCreateImage(m_Spec.Allocator, &createInfo, &allocInfo, &m_Image, &m_Allocation,
                           nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }

        if (m_Spec.SafeLayout != createInfo.initialLayout) {
            auto queue = Renderer::GetGraphicsQueue();
            if (queue) {
                auto& cmdlist = queue->RT_Get();
                cmdlist.RT_Begin();

                VkImageSubresourceRange subresource{};
                subresource.aspectMask = m_Spec.AspectFlags;
                subresource.levelCount = m_Spec.MipLevels;
                subresource.layerCount = m_Spec.ArrayLayers;

                auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();
                RT_TransitionLayout(cmdBuffer, m_Image, createInfo.initialLayout,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_Spec.SafeLayout,
                                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, subresource);

                cmdlist.AddSemaphore(SignalUsed(), SemaphoreUsage::Signal);

                cmdlist.RT_End();
                queue->RT_Submit(cmdlist);
            } else {
                FUUJIN_ERROR("Attempted to transition image upon creation, however no graphics "
                             "queue was found on the renderer! Resetting safe layout to UNDEFINED");
            }
        }
    }

    void VulkanImage::RT_CreateView() {
        ZoneScoped;

        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_Image;
        createInfo.viewType = m_Spec.ViewType;
        createInfo.format = m_Spec.Format;
        createInfo.flags = m_Spec.ViewFlags;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        createInfo.subresourceRange.aspectMask = m_Spec.AspectFlags;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = m_Spec.MipLevels;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = m_Spec.ArrayLayers;

        if (vkCreateImageView(m_Device->GetDevice(), &createInfo,
                              &VulkanContext::GetAllocCallbacks(), &m_View) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    struct TransitionSemaphoreData {
        VulkanCommandBuffer* Buffer;
        Ref<VulkanImage> Image;
    };

    ImageTransition::ImageTransition(VulkanCommandBuffer& cmdBuffer, const Ref<VulkanImage>& image,
                                     VkImageLayout layout, VkPipelineStageFlags stage,
                                     VkPipelineStageFlags endStage) {
        ZoneScoped;

        const auto& spec = image->GetSpec();
        m_Subresource.aspectMask = spec.AspectFlags;
        m_Subresource.baseArrayLayer = 0;
        m_Subresource.layerCount = spec.ArrayLayers;
        m_Subresource.baseMipLevel = 0;
        m_Subresource.levelCount = spec.MipLevels;

        VkImage vkImage = image->GetImage();
        VkImageLayout safeLayout = image->GetSpec().SafeLayout;
        VkImageSubresourceRange subresource = m_Subresource;

        m_Buffer = &cmdBuffer;
        m_Image = image;

        m_Layout = layout;
        m_EndStage = endStage != 0 ? endStage : stage;

        auto semaphoreData = new TransitionSemaphoreData;
        semaphoreData->Buffer = m_Buffer;
        semaphoreData->Image = m_Image;

        auto vkCmdBuffer = m_Buffer->Get();
        Renderer::Submit([=]() {
            VulkanImage::RT_TransitionLayout(vkCmdBuffer, vkImage, safeLayout,
                                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, layout, stage,
                                             subresource);

            auto semaphore = semaphoreData->Image->GetSignaledSemaphore();
            if (semaphore.IsPresent()) {
                semaphoreData->Buffer->AddSemaphore(semaphore, SemaphoreUsage::Wait);
            }

            delete semaphoreData;
        });
    }

    ImageTransition::~ImageTransition() {
        ZoneScoped;

        VkImageLayout safeLayout = m_Image->GetSpec().SafeLayout;
        if (safeLayout != VK_IMAGE_LAYOUT_UNDEFINED) {
            auto semaphoreData = new TransitionSemaphoreData;
            semaphoreData->Buffer = m_Buffer;
            semaphoreData->Image = m_Image;

            VkCommandBuffer cmdBuffer = m_Buffer->Get();
            VkImage vkImage = m_Image->GetImage();
            VkImageSubresourceRange subresource = m_Subresource;

            VkImageLayout layout = m_Layout;
            VkPipelineStageFlags stage = m_EndStage;

            Renderer::Submit([=]() {
                VulkanImage::RT_TransitionLayout(cmdBuffer, vkImage, layout, stage, safeLayout,
                                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, subresource);

                semaphoreData->Buffer->AddSemaphore(semaphoreData->Image->SignalUsed(),
                                                    SemaphoreUsage::Signal);

                delete semaphoreData;
            });
        }
    }
} // namespace fuujin
