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

    bool AssetManager::LoadPath(const fs::path& path, const fs::path& key) {
        ZoneScoped;

        auto pathText = path.lexically_normal().string();
        if (!path.has_extension()) {
            FUUJIN_WARN("Asset path {} has no extension!", pathText.c_str());
            return false;
        }

        auto extension = path.extension();
        auto extensionText = extension.string().substr(1);

        if (!s_Data->ExtensionMap.contains(extensionText)) {
            FUUJIN_WARN("Asset extension {} not found!", extensionText.c_str());
            return false;
        }

        auto type = s_Data->ExtensionMap.at(extensionText);
        auto& typeData = s_Data->AssetTypes[type];

        auto asset = typeData.Serializer->Deserialize(path);
        if (!asset) {
            return false;
        }

        auto assetPath = NormalizePath(key);
        typeData.Assets[assetPath] = asset;
        s_Data->PathTypeMap[assetPath] = type;

        return true;
    }

    void AssetManager::LoadDirectory(const fs::path& directory,
                                     const std::optional<fs::path>& pathPrefix) {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        auto directoryPath = fs::absolute(directory);
        auto pathText = directoryPath.lexically_normal().string();
        FUUJIN_INFO("Loading asset directory: {}", pathText.c_str());

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

            pathText = fullPath.lexically_normal().string();
            if (LoadPath(fullPath, assetPath)) {
                FUUJIN_INFO("Loaded asset: {}", pathText.c_str());
            } else {
                FUUJIN_ERROR("Failed to load asset: {}", pathText.c_str());
            }
        }
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