#pragma once

#include "fuujin/asset/ModelSource.h"
#include "fuujin/renderer/Model.h"

struct aiScene;
struct aiNode;
struct aiMesh;
struct aiBone;
struct aiAnimation;

namespace fuujin {
    class ModelImporter {
    public:
        ModelImporter();
        ~ModelImporter() = default;

        ModelImporter(const ModelImporter&) = delete;
        ModelImporter& operator=(const ModelImporter&) = delete;

        std::optional<fs::path> Import(const Ref<ModelSource>& source);

    private:
        struct ImportedBone {
            std::optional<size_t> Existing;
            bool Processed;

            std::string Name;
            aiBone* Pointer;
        };

        void ProcessNode(aiNode* node);
        void ProcessMesh(aiMesh* mesh);
        
        void ProcessBone(aiNode* node, const ImportedBone& imported, Armature* armature,
                         std::vector<BoneReference>& bones);

        void ProcessAnimation(aiAnimation* animation);

        Ref<Material> GetMaterial(unsigned int index);
        size_t GetArmature(aiNode* node);

        Ref<Model> m_Model;
        const aiScene* m_Scene;
        fs::path m_SourcePath;

        fs::path m_ModelDirectory;
        fs::path m_MaterialDirectory, m_MaterialPrefix;
        fs::path m_AnimationDirectory, m_AnimationPrefix;
        std::map<unsigned int, Ref<Material>> m_Materials;

        std::unordered_map<aiNode*, size_t> m_NodeMap;
        std::unordered_map<aiNode*, size_t> m_ArmatureMap;
        std::unordered_map<aiNode*, ImportedBone> m_ImportedBones;
    };
} // namespace fuujin