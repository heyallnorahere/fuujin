#include "fuujinpch.h"
#include "fuujin/asset/ModelSource.h"

#include <assimp/Importer.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace fuujin {
    // see CMakePresets.json
    // only enabled importer
    static const std::vector<std::string> s_ModelExtensions = { "gltf", "glb" };

    ModelSource::ModelSource(const fs::path& path, Assimp::Importer* importer,
                             const aiScene* scene) {
        ZoneScoped;

        m_Path = path;

        m_Importer = importer;
        m_Scene = scene;
    }

    ModelSource::~ModelSource() {
        ZoneScoped;

        delete m_Importer;
    }

    Ref<Asset> ModelSourceSerializer::Deserialize(const fs::path& path) const {
        ZoneScoped;

        static constexpr uint32_t flags = aiProcess_Triangulate | aiProcess_GenNormals |
                                          aiProcess_ValidateDataStructure |
                                          aiProcess_LimitBoneWeights | aiProcess_CalcTangentSpace;

        auto pathText = path.string();
        auto importer = new Assimp::Importer;
        auto scene = importer->ReadFile(pathText, flags);

        if (scene == nullptr) {
            FUUJIN_ERROR("Scene at path {} could not be loaded!", pathText.c_str());
            return nullptr;
        }

        if ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0) {
            FUUJIN_ERROR("Scene loaded from path {} is incomplete!", pathText.c_str());
            return nullptr;
        }

        return Ref<ModelSource>::Create(path, importer, scene);
    }

    bool ModelSourceSerializer::Serialize(const Ref<Asset>& asset) const {
        ZoneScoped;

        FUUJIN_WARN("No point in trying to serialize a model source");
        return false;
    }

    const std::vector<std::string>& ModelSourceSerializer::GetExtensions() const {
        return s_ModelExtensions;
    }
} // namespace fuujin