#pragma once
#include "fuujin/asset/Asset.h"

namespace Assimp {
    class Importer;
} // namespace Assimp

struct aiScene;

namespace fuujin {
    class ModelSource : public Asset {
    public:
        ModelSource(const fs::path& path, Assimp::Importer* importer, const aiScene* scene);
        virtual ~ModelSource() override;

        virtual const fs::path& GetPath() const override { return m_Path; }
        virtual AssetType GetAssetType() const override { return AssetType::ModelSource; };

        const aiScene* GetScene() const { return m_Scene; }

    private:
        fs::path m_Path;

        Assimp::Importer* m_Importer;
        const aiScene* m_Scene;
    };

    class ModelSourceSerializer : public AssetSerializer {
    public:
        virtual Ref<Asset> Deserialize(const fs::path& path) const override;
        virtual bool Serialize(const Ref<Asset>& asset) const override;

        virtual const std::vector<std::string>& GetExtensions() const override;
        virtual AssetType GetType() const override { return AssetType::ModelSource; }
    };
} // namespace fuujin