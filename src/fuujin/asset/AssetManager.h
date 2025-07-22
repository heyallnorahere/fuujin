#pragma once
#include "fuujin/asset/Asset.h"

namespace fuujin {
    class AssetManager {
    public:
        static constexpr char PreferredSeparator = '/';

        AssetManager() = delete;

        static void SHutdown();

        static fs::path NormalizePath(const fs::path& path);
        static std::optional<AssetType> DetermineAssetType(const fs::path& path);

        static bool LoadPath(const fs::path& realPath, const fs::path& virtualPath);
        static bool AddAsset(const Ref<Asset>& asset, const fs::path& virtualPath);

        static void LoadDirectory(const fs::path& directory,
                                  const std::optional<fs::path>& pathPrefix = {});

        static std::optional<fs::path> GetVirtualPath(const fs::path& real);

        static void RegisterAssetType(std::unique_ptr<AssetSerializer>&& serializer);
        static const AssetSerializer* GetSerializer(AssetType type);

        template <typename _Ty, typename... Args>
        static void RegisterAssetType(Args&&... args) {
            ZoneScoped;
            static_assert(std::is_base_of_v<AssetSerializer, _Ty>,
                          "Passed type does not extend AssetSerializer!");

            auto instance = new _Ty(std::forward(args)...);
            RegisterAssetType(std::unique_ptr<AssetSerializer>(instance));
        }

        static bool AssetExists(const fs::path& path, bool isPathVirtual = true);
        static Ref<Asset> GetAsset(const fs::path& path);

        template <typename _Ty>
        static Ref<_Ty> GetAsset(const fs::path& path) {
            ZoneScoped;
            static_assert(std::is_base_of_v<Asset, _Ty>, "Passed type does not extend Asset!");

            Ref<Asset> asset = GetAsset(path);
            if (!asset) {
                return nullptr;
            }

            if constexpr (!std::is_same_v<Asset, _Ty>) {
                if (GetAssetType<_Ty>() != asset->GetAssetType()) {
                    throw std::runtime_error("Invalid asset cast!");
                }
            }

            return asset.As<_Ty>();
        }
    };
} // namespace fuujin
