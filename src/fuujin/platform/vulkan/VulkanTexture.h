#pragma once
#include "fuujin/renderer/Texture.h"
#include "fuujin/platform/vulkan/Vulkan.h"

#include "fuujin/platform/vulkan/VulkanImage.h"
#include "fuujin/platform/vulkan/VulkanDevice.h"

namespace fuujin {
    class VulkanSampler : public Sampler {
    public:
        static VkFilter ConvertFilter(SamplerFilter filter);
        static VkSamplerAddressMode ConvertAddressMode(AddressMode mode);

        VulkanSampler(const Ref<VulkanDevice>& device, const Spec& spec);
        virtual ~VulkanSampler() override;

        virtual const Spec& GetSpec() const override { return m_Spec; }

        VkSampler GetSampler() const { return m_Sampler; }

    private:
        void RT_CreateSampler();

        Ref<VulkanDevice> m_Device;
        Spec m_Spec;

        VkSampler m_Sampler;
    };

    class VulkanTexture : public Texture {
    public:
        static VkFormat ConvertFormat(Format format);

        VulkanTexture(const Ref<VulkanDevice>& device, const VmaAllocator& allocator,
                      const Spec& spec);

        virtual ~VulkanTexture() override = default;

        virtual const Spec& GetSpec() const override { return m_Spec; }
        virtual Ref<DeviceImage> GetImage() const override { return m_Image; }

        virtual const fs::path& GetPath() const override { return m_Spec.Path; }

        Ref<VulkanImage> GetVulkanImage() const { return m_Image; }

    private:
        void CreateImage(VmaAllocator allocator);

        Ref<VulkanDevice> m_Device;
        Spec m_Spec;

        Ref<VulkanImage> m_Image;
    };

    template <>
    inline std::optional<AssetType> GetAssetType<VulkanTexture>() {
        return GetAssetType<Texture>();
    }
} // namespace fuujin