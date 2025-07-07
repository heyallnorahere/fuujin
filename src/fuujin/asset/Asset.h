#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    enum class AssetType { Texture, Material, ModelSource, Model };

    template <typename _Ty>
    inline std::optional<AssetType> GetAssetType() {
        return {};
    }

    class Asset : public RefCounted {
    public:
        virtual const fs::path& GetPath() const = 0;
        virtual AssetType GetAssetType() const = 0;
    };

    class AssetSerializer {
    public:
        virtual ~AssetSerializer() = default;

        virtual Ref<Asset> Deserialize(const fs::path& path) const = 0;
        virtual bool Serialize(const Ref<Asset>& asset) const = 0;

        virtual const std::vector<std::string>& GetExtensions() const = 0;
        virtual AssetType GetType() const = 0;
    };
} // namespace fuujin