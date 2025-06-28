#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    enum class ShaderStage { Vertex, Fragment, Geometry, Compute };

    class GPUType {
    public:
        struct Field {
            std::shared_ptr<GPUType> Type;
            size_t Offset, Stride;
            std::vector<size_t> Dimensions;
        };

        virtual ~GPUType() = default;

        virtual const std::string& GetName() const = 0;
        virtual size_t GetSize() const = 0;
        virtual const std::unordered_map<std::string, Field>& GetFields() const = 0;
    };

    class GPUResource {
    public:
        enum Type : uint32_t {
            UniformBuffer = 1 << 0,
            StorageBuffer = 1 << 1,
            SampledImage = 1 << 2,
            ShaderSampler = 1 << 3,
            StorageImage = 1 << 4,
        };

        virtual ~GPUResource() = default;

        virtual const std::string& GetName() const = 0;
        virtual uint32_t GetResourceType() const = 0;
        virtual const std::vector<size_t>& GetDimensions() const = 0;

        virtual void GetStages(std::unordered_set<ShaderStage>& stages) const = 0;
        virtual std::shared_ptr<GPUType> GetType(
            const std::optional<ShaderStage>& stage = {}) const = 0;
    };

    class Shader : public RefCounted {
    public:
        using Code = std::unordered_map<ShaderStage, std::vector<uint8_t>>;

        virtual uint64_t GetID() const = 0;

        virtual void GetStages(std::unordered_set<ShaderStage>& stages) const = 0;

        virtual std::shared_ptr<GPUResource> GetResourceByName(const std::string& name) const = 0;
        virtual std::shared_ptr<GPUResource> GetPushConstants() const = 0;
    };
} // namespace fuujin