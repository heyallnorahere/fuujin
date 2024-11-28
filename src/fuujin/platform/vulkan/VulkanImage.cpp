#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanImage.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    VulkanImage::VulkanImage(const Ref<VulkanDevice>& device, const VulkanSpec& spec)
        : m_Device(device), m_Spec(spec) {
        ZoneScoped;

        m_Image = spec.ExistingImage;
        if (m_Image == VK_NULL_HANDLE) {
            Renderer::Submit([this]() mutable {
                ZoneScoped;

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

                std::vector<uint32_t> indices;
                createInfo.sharingMode = m_Device->GetSharingMode(m_Spec.QueueOwnership, indices);

                if (indices.size() > 0) {
                    createInfo.queueFamilyIndexCount = (uint32_t)indices.size();
                    createInfo.pQueueFamilyIndices = indices.data();
                }

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

                if (vmaCreateImage(m_Spec.Allocator, &createInfo, &allocInfo, &m_Image,
                                   &m_Allocation, nullptr) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create image!");
                }
            });
        }

        Renderer::Submit([this]() mutable {
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

            if (vkCreateImageView(m_Device->GetDevice(), &createInfo, &VulkanContext::GetAllocCallbacks(), &m_View) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view!");
            }
        });

        // todo: yada yada layouts and barriers
    }

    VulkanImage::~VulkanImage() {
        Renderer::Submit([this]() mutable {
            
        });
    }
} // namespace fuujin