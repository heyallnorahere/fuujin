#include "fuujinpch.h"
#include "fuujin/asset/AssetManager.h"

namespace fuujin {
    struct AssetTypeData {
        std::unique_ptr<AssetSerializer> Serializer;
        std::unordered_map<fs::path, Ref<Asset>> Assets;
    };

    struct AssetManagerData {
        std::unordered_map<AssetType, AssetTypeData> AssetTypes;
        std::unordered_map<fs::path, AssetType> PathTypeMap;
        std::unordered_map<std::string, AssetType> ExtensionMap;
        std::unordered_map<fs::path, fs::path> RealToVirtual;
    };

    static std::unique_ptr<AssetManagerData> s_Data;

    void AssetManager::Clear() {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        s_Data->PathTypeMap.clear();
        for (auto& [type, data] : s_Data->AssetTypes) {
            data.Assets.clear();
        }
    }

    fs::path AssetManager::NormalizePath(const fs::path& path) {
        ZoneScoped;
        auto result = path.lexically_normal();

        auto systemSeparator = (char)fs::path::preferred_separator;
        if (PreferredSeparator != systemSeparator) {
            auto pathText = result.string();

            std::stringstream ss;
            ss << PreferredSeparator;

            size_t position = pathText.find(systemSeparator);
            while (position != std::string::npos) {
                pathText.replace(position, 1, ss.str());
                position = pathText.find(systemSeparator, position + 1);
            }

            result = pathText;
        }

        return result;
    }

    std::optional<AssetType> AssetManager::DetermineAssetType(const fs::path& path) {
        ZoneScoped;
        if (!path.has_extension()) {
            return {};
        }

        auto extension = path.extension();
        auto extensionText = extension.string().substr(1);

        if (!s_Data || !s_Data->ExtensionMap.contains(extensionText)) {
            return {};
        }

        return s_Data->ExtensionMap.at(extensionText);
    }

    bool AssetManager::LoadPath(const fs::path& realPath, const fs::path& virtualPath) {
        ZoneScoped;

        auto pathText = realPath.lexically_normal().string();
        auto assetType = DetermineAssetType(realPath);

        if (!assetType.has_value()) {
            FUUJIN_WARN("Could not determine asset type of path {}", pathText.c_str());
            return false;
        }

        auto type = assetType.value();
        auto& typeData = s_Data->AssetTypes[type];

        auto asset = typeData.Serializer->Deserialize(realPath);
        if (!asset) {
            return false;
        }

        auto assetPath = NormalizePath(virtualPath);
        typeData.Assets[assetPath] = asset;
        s_Data->PathTypeMap[assetPath] = type;
        s_Data->RealToVirtual[realPath] = virtualPath;

        return true;
    }

    struct LoadData {
        fs::path Real, Virtual;
    };

    void AssetManager::LoadDirectory(const fs::path& directory,
                                     const std::optional<fs::path>& pathPrefix) {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        static const std::vector<AssetType> loadOrder = { AssetType::Texture, AssetType::Material };

        auto directoryPath = fs::absolute(directory);
        auto directoryText = directoryPath.lexically_normal().string();
        FUUJIN_INFO("Scanning asset directory: {}", directoryText.c_str());

        size_t assetCount = 0;
        std::map<AssetType, std::vector<LoadData>> loadList;
        for (const auto& entry : fs::recursive_directory_iterator(directoryPath)) {
            if (!entry.is_directory()) {
                continue;
            }

            fs::path fullPath = entry.path();
            fs::path assetPath;
            if (pathPrefix.has_value()) {
                assetPath = pathPrefix.value() / fullPath.lexically_relative(directory);
            } else {
                assetPath = fs::relative(fullPath);
            }

            auto pathText = fullPath.lexically_normal().string();
            FUUJIN_DEBUG("Discovered {}", pathText.c_str());

            auto type = DetermineAssetType(fullPath);
            if (!type.has_value()) {
                FUUJIN_WARN("Could not determine asset type of {} - skipping load",
                            pathText.c_str());

                continue;
            }

            assetCount++;
            auto& data = loadList[type.value()].emplace_back();

            data.Real = fullPath;
            data.Virtual = assetPath;
        }

        FUUJIN_INFO("Discovered {} assets to be loaded in directory {}", assetCount,
                    directoryText.c_str());

        size_t assetsLoaded = 0;
        for (auto type : loadOrder) {
            if (!loadList.contains(type)) {
                continue;
            }

            for (const auto& paths : loadList.at(type)) {
                auto pathText = paths.Real.lexically_normal().string();
                if (LoadPath(paths.Real, paths.Virtual)) {
                    FUUJIN_INFO("Loaded asset: {}", pathText.c_str());
                    assetsLoaded++;
                } else {
                    FUUJIN_ERROR("Failed to load asset: {}", pathText.c_str());
                }
            }
        }

        FUUJIN_INFO("Successfully loaded {} assets from directory {}", assetsLoaded,
                    directoryText.c_str());
    }

    std::optional<fs::path> AssetManager::GetVirtualPath(const fs::path& real) {
        ZoneScoped;

        if (!s_Data->RealToVirtual.contains(real)) {
            return {};
        }

        return s_Data->RealToVirtual.at(real);
    }

    void AssetManager::RegisterAssetType(AssetType type,
                                         std::unique_ptr<AssetSerializer>&& serializer) {
        ZoneScoped;
        if (!s_Data) {
            s_Data = std::make_unique<AssetManagerData>();
        }

        if (s_Data->AssetTypes.contains(type)) {
            throw std::runtime_error("Asset type already registered!");
        }

        s_Data->AssetTypes[type].Serializer = std::move(serializer);
    }

    Ref<Asset> AssetManager::GetAsset(const fs::path& path) {
        ZoneScoped;
        if (!s_Data) {
            return nullptr;
        }

        if (!s_Data->PathTypeMap.contains(path)) {
            return nullptr;
        }

        auto type = s_Data->PathTypeMap.at(path);
        return s_Data->AssetTypes.at(type).Assets.at(path);
    }
} // namespace fuujin