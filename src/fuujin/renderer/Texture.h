#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/DeviceImage.h"

namespace fuujin {
    enum class SamplerFilter { Linear = 0, Nearest };
    enum class AddressMode { Repeat = 0, MirroredRepeat, ClampToEdge, ClampToBorder };

    class Sampler : public RefCounted {
    public:
        struct Spec {
            SamplerFilter Min, Mag, Mipmap;
            AddressMode U, V, W;
        };

        virtual const Spec& GetSpec() const = 0;
    };

    class Texture : public RefCounted {
    public:
        enum class Format { RGBA8 = 0 };
        enum class Type { _2D = 0, _3D, Cube };
        enum class Feature { ShaderStorage, ColorAttachment, DepthAttachment, Transfer };

        struct Spec {
            Ref<Sampler> Sampler;

            uint32_t Width, Height, Depth;
            uint32_t MipLevels;
            Format Format;
            Type Type;
            std::set<Feature> AdditionalFeatures;
        };

        virtual const Spec& GetSpec() const = 0;
        virtual Ref<DeviceImage> GetImage() const = 0;
    };
} // namespace fuujin