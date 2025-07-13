#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanTexture.h"

#include "fuujin/renderer/Renderer.h"

#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    static uint64_t s_TextureID = 0;

    VkFilter VulkanSampler::ConvertFilter(SamplerFilter filter) {
        switch (filter) {
        case SamplerFilter::Linear:
            return VK_FILTER_LINEAR;
        case SamplerFilter::Nearest:
            return VK_FILTER_NEAREST;
        default:
            throw std::runtime_error("Invalid sampler filter!");
        }
    }

    VkSamplerAddressMode VulkanSampler::ConvertAddressMode(AddressMode mode) {
        switch (mode) {
        case AddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case AddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case AddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case AddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            throw std::runtime_error("Invalid address mode!");
        }
    }

    VkFormat VulkanTexture::ConvertFormat(Format format) {
        switch (format) {
        case Format::RGBA8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::RGB8:
            return VK_FORMAT_R8G8B8_UNORM;
        default:
            throw std::runtime_error("Invalid texture format!");
        }
    }

    VulkanSampler::VulkanSampler(const Ref<VulkanDevice>& device, const Spec& spec) {
        ZoneScoped;

        m_Device = device;
        m_Spec = spec;

        Renderer::Submit([this]() { RT_CreateSampler(); });
    }

    VulkanSampler::~VulkanSampler() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto sampler = m_Sampler;

        Renderer::Submit(
            [=]() { vkDestroySampler(device, sampler, &VulkanContext::GetAllocCallbacks()); });
    }

    void VulkanSampler::RT_CreateSampler() {
        ZoneScoped;

        VkSamplerCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.magFilter = ConvertFilter(m_Spec.Mag);
        createInfo.minFilter = ConvertFilter(m_Spec.Min);
        createInfo.addressModeU = ConvertAddressMode(m_Spec.U);
        createInfo.addressModeV = ConvertAddressMode(m_Spec.V);
        createInfo.addressModeW = ConvertAddressMode(m_Spec.W);

        switch (m_Spec.Mipmap) {
        case SamplerFilter::Linear:
            createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case SamplerFilter::Nearest:
            createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        default:
            throw std::runtime_error("Invalid mipmap filter!");
        }

        VkPhysicalDeviceFeatures2 features{};
        m_Device->RT_GetFeatures(features);

        if (features.features.samplerAnisotropy) {
            VkPhysicalDeviceProperties2 properties{};
            m_Device->RT_GetProperties(properties);

            createInfo.anisotropyEnable = VK_TRUE;
            createInfo.maxAnisotropy = properties.properties.limits.maxSamplerAnisotropy;
        }

        if (vkCreateSampler(m_Device->GetDevice(), &createInfo, &VulkanContext::GetAllocCallbacks(),
                            &m_Sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sampler!");
        }
    }

    VulkanTexture::VulkanTexture(const Ref<VulkanDevice>& device,
                                 const VmaAllocator& allocator, const Spec& spec) {
        ZoneScoped;

        m_ID = s_TextureID++;

        m_Device = device;
        m_Spec = spec;

        Renderer::Submit([this, &allocator]() { CreateImage(allocator); });
    }

    void VulkanTexture::CreateImage(VmaAllocator allocator) {
        VulkanImage::VulkanSpec spec;
        spec.Allocator = allocator;
        spec.Extent.width = m_Spec.Width;
        spec.Extent.height = m_Spec.Height;
        spec.Extent.depth = m_Spec.Depth;
        spec.Format = ConvertFormat(m_Spec.ImageFormat);
        spec.Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        spec.MipLevels = m_Spec.MipLevels;
        spec.ArrayLayers = 1;
        spec.SafeLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        spec.AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT; // todo: depth?
        spec.Samples = VK_SAMPLE_COUNT_1_BIT;         // todo: multisampling attachments?

        switch (m_Spec.TextureType) {
        case Type::_2D:
            spec.Type = VK_IMAGE_TYPE_2D;
            spec.ViewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case Type::_3D:
            spec.Type = VK_IMAGE_TYPE_3D;
            spec.ViewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        case Type::Cube:
            spec.Type = VK_IMAGE_TYPE_2D;
            spec.ViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            spec.ArrayLayers = 6;
            spec.Flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            break;
        default:
            throw std::runtime_error("Invalid texture type!");
        }

        m_Image = Ref<VulkanImage>::Create(m_Device, spec);
    }
} // namespace fuujin