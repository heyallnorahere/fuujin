#pragma once
#include "fuujin/asset/Asset.h"

#include "fuujin/renderer/DeviceImage.h"

namespace fuujin {
    enum class SamplerFilter { Linear = 0, Nearest };
    enum class AddressMode { Repeat = 0, MirroredRepeat, ClampToEdge, ClampToBorder };

    enum class BorderColor {
        FloatTransparentBlack = 0,
        IntTransparentBlack,
        FloatOpaqueBlack,
        IntOpaqueBlack,
        FloatOpaqueWhite,
        IntOpaqueWhite,
    };

    class Sampler : public RefCounted {
    public:
        struct Spec {
            Spec() {
                std::memset(this, 0, sizeof(Spec));
            }

            SamplerFilter Min, Mag, Mipmap;
            AddressMode U, V, W;
            BorderColor Border;
        };

        virtual const Spec& GetSpec() const = 0;
    };

    class Texture : public Asset {
    public:
        enum class Format { RGBA8 = 0, RGB8, A8, D32 };
        enum class Type { _2D = 0, _3D, Cube };
        enum class Feature { ShaderStorage, ColorAttachment, DepthAttachment, Transfer };

        struct Spec {
            Ref<Sampler> TextureSampler;
            fs::path Path;

            uint32_t Width, Height, Depth;
            uint32_t MipLevels;
            uint32_t Samples;
            Format ImageFormat;
            Type TextureType;
            std::set<Feature> AdditionalFeatures;
        };

        virtual uint64_t GetID() const = 0;

        virtual const Spec& GetSpec() const = 0;
        virtual Ref<DeviceImage> GetImage() const = 0;

        virtual AssetType GetAssetType() const override { return AssetType::Texture; }
    };

    template <>
    inline std::optional<AssetType> GetAssetType<Texture>() {
        return AssetType::Texture;
    }

    class TextureSerializer : public AssetSerializer {
    public:
        virtual Ref<Asset> Deserialize(const fs::path& path) const override;
        virtual bool Serialize(const Ref<Asset>& asset) const override;

        virtual const std::vector<std::string>& GetExtensions() const override;
        virtual AssetType GetType() const override;
    };
} // namespace fuujin
