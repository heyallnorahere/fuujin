#include "fuujinpch.h"
#include "fuujin/asset/ModelImporter.h"

#include "fuujin/asset/AssetManager.h"

#include "fuujin/animation/Animation.h"

#include <assimp/scene.h>

namespace fuujin {
    template <typename _Ty>
    static void ConvertAssimpMatrix(const aiMatrix4x4t<_Ty>& mat,
                                    glm::mat<4, 4, _Ty, glm::defaultp>& result) {
        ZoneScoped;

        // row major to column major
        // aiMatrix4x4t<_Ty> is row major (mat.{row}{column})
        // glm::mat<4, 4, _Ty, Q> is column major (result[{column}][{row}])

        // row 1
        result[0][0] = mat.a1;
        result[1][0] = mat.a2;
        result[2][0] = mat.a3;
        result[3][0] = mat.a4;

        // row 2
        result[0][1] = mat.b1;
        result[1][1] = mat.b2;
        result[2][1] = mat.b3;
        result[3][1] = mat.b4;

        // row 3
        result[0][2] = mat.c1;
        result[1][2] = mat.c2;
        result[2][2] = mat.c3;
        result[3][2] = mat.c4;

        // row 4
        result[0][3] = mat.d1;
        result[1][3] = mat.d2;
        result[2][3] = mat.d3;
        result[3][3] = mat.d4;
    }

    template <typename _Ty>
    static glm::vec<4, _Ty, glm::defaultp> ConvertAssimpColor(const aiColor4t<_Ty>& color) {
        ZoneScoped;

        glm::vec<4, _Ty, glm::defaultp> result;
        result.r = color.r;
        result.g = color.g;
        result.b = color.b;
        result.a = color.a;

        return result;
    }

    ModelImporter::ModelImporter() {
        ZoneScoped;

        m_Scene = nullptr;
    }

    std::optional<fs::path> ModelImporter::Import(const Ref<ModelSource>& source) {
        ZoneScoped;

        m_SourcePath = source->GetPath();
        auto virtualSourcePath = AssetManager::GetVirtualPath(m_SourcePath);

        auto sourcePathText = m_SourcePath.string();
        if (!virtualSourcePath.has_value()) {
            FUUJIN_ERROR("Model source at path {} is not registered! Aborting import",
                         sourcePathText.c_str());

            return {};
        }

        m_ModelDirectory = m_SourcePath.parent_path();
        auto modelPrefix = virtualSourcePath.value().parent_path();

        static const std::string materialDirectorySuffix = "materials";
        m_MaterialDirectory = m_ModelDirectory / materialDirectorySuffix;
        m_MaterialPrefix = AssetManager::NormalizePath(modelPrefix / materialDirectorySuffix);
        fs::create_directories(m_MaterialDirectory);

        static const std::string animationDirectorySuffix = "animations";
        m_AnimationDirectory = m_ModelDirectory / animationDirectorySuffix;
        m_AnimationPrefix = AssetManager::NormalizePath(modelPrefix / animationDirectorySuffix);
        fs::create_directories(m_AnimationDirectory);

        auto sourceFilename = m_SourcePath.filename();
        auto modelFilename = sourceFilename.replace_extension(".model");
        auto modelPath = m_ModelDirectory / modelFilename;
        auto virtualModelPath = modelPrefix / modelFilename;

        auto resultPathText = modelPath.string();
        FUUJIN_INFO("Importing model at {} to {}", sourcePathText.c_str(), resultPathText.c_str());

        m_Scene = source->GetScene();
        m_Model = Ref<Model>::Create(modelPath);

        try {
            ProcessNode(m_Scene->mRootNode);

            for (unsigned int i = 0; i < m_Scene->mNumMeshes; i++) {
                auto mesh = m_Scene->mMeshes[i];
                ProcessMesh(mesh);
            }

            for (unsigned int i = 0; i < m_Scene->mNumAnimations; i++) {
                auto animation = m_Scene->mAnimations[i];
                ProcessAnimation(animation);
            }
        } catch (const std::runtime_error& exc) {
            FUUJIN_ERROR("Model import error: {}", exc.what());
            return {};
        }

        if (!AssetManager::AddAsset(m_Model, virtualModelPath)) {
            FUUJIN_ERROR("Failed to add model to registry!");
            return {};
        }

        m_Model.Reset();
        m_Materials.clear();
        m_NodeMap.clear();
        m_ArmatureMap.clear();

        return virtualModelPath;
    }

    void ModelImporter::ProcessNode(aiNode* node) {
        ZoneScoped;

        if (m_NodeMap.contains(node)) {
            FUUJIN_DEBUG("Node {} already processed - skipping", node->mName.C_Str());
            return;
        }

        glm::mat4 nodeTransform;
        ConvertAssimpMatrix(node->mTransformation, nodeTransform);

        std::optional<size_t> parent;
        if (node->mParent != nullptr) {
            parent = m_NodeMap.at(node->mParent);
        }

        std::unordered_set<size_t> meshes;
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            auto meshIndex = node->mMeshes[i];
            meshes.insert((size_t)meshIndex);
        }

        m_NodeMap[node] = m_Model->AddNode(node->mName.C_Str(), nodeTransform, meshes, parent);

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            auto child = node->mChildren[i];
            ProcessNode(child);
        }
    }

    void ModelImporter::ProcessMesh(aiMesh* mesh) {
        ZoneScoped;

        bool hasUV = mesh->HasTextureCoords(0);
        if (!hasUV) {
            FUUJIN_WARN("Mesh has no UV coordinates!");
        }

        bool hasNormals = mesh->HasNormals();
        if (!hasNormals) {
            FUUJIN_WARN("Mesh has no normals!");
        }

        bool hasTangents = mesh->HasTangentsAndBitangents();
        if (!hasTangents) {
            FUUJIN_WARN("Mesh has no tangents!");
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            FUUJIN_TRACE("Vertex {}", i);
            auto& vertex = vertices.emplace_back(Vertex{});

            auto position = mesh->mVertices[i];
            FUUJIN_TRACE("Position: <{}, {}, {}>", position.x, position.y, position.z);

            vertex.Position.x = position.x;
            vertex.Position.y = position.y;
            vertex.Position.z = position.z;

            if (hasUV) {
                auto uv = mesh->mTextureCoords[0][i];
                FUUJIN_TRACE("UV: <{}, {}>", uv.x, uv.y);

                vertex.UV.x = uv.x;
                vertex.UV.y = uv.y;
            }

            if (hasNormals) {
                auto normal = mesh->mNormals[i];
                FUUJIN_TRACE("Normal: <{}, {}, {}>", normal.x, normal.y, normal.z);

                vertex.Normal.x = normal.x;
                vertex.Normal.y = normal.y;
                vertex.Normal.z = normal.z;
            }

            if (hasTangents) {
                auto tangent = mesh->mTangents[i];
                FUUJIN_TRACE("Tangent: <{}, {}, {}>", tangent.x, tangent.y, tangent.z);

                vertex.Tangent.x = tangent.x;
                vertex.Tangent.y = tangent.y;
                vertex.Tangent.z = tangent.z;
            }
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            const auto& face = mesh->mFaces[i];
            if (face.mNumIndices != Mesh::IndicesPerFace) {
                FUUJIN_ERROR("Mesh not triangulated! Skipping mesh");
                return;
            }

            FUUJIN_TRACE("Face {}", i);
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                uint32_t index = (uint32_t)face.mIndices[j];
                FUUJIN_TRACE("Index {}: {}", j, index);

                indices.push_back(index);
            }
        }

        size_t armatureIndex = 0;
        aiNode* armatureNode = nullptr;

        const auto& armatures = m_Model->GetArmatures();
        Armature* armature = nullptr;

        for (unsigned int i = 0; i < mesh->mNumBones; i++) {
            auto bone = mesh->mBones[i];

            if (armatureNode == nullptr) {
                armatureNode = bone->mArmature;
                armatureIndex = GetArmature(armatureNode);
                armature = armatures[armatureIndex].get();
            } else if (armatureNode != bone->mArmature) {
                throw std::runtime_error("Cannot use more than one armature per mesh!");
            }

            auto& imported = m_ImportedBones[bone->mNode];
            imported.Processed = false;
            imported.Name = bone->mName.C_Str();
            imported.Existing = armature->FindBoneByName(imported.Name);
            imported.Pointer = bone;
        }

        std::vector<BoneReference> bones;
        for (const auto& [node, imported] : m_ImportedBones) {
            ProcessBone(node, imported, armature, bones);
        }

        auto material = GetMaterial(mesh->mMaterialIndex);
        auto result = std::make_unique<Mesh>(material, vertices, indices, bones, armatureIndex);

        m_ImportedBones.clear();
        m_Model->AddMesh(std::move(result));
    }

    void ModelImporter::ProcessBone(aiNode* node, const ImportedBone& imported, Armature* armature,
                                    std::vector<BoneReference>& bones) {
        ZoneScoped;
        FUUJIN_TRACE("Processing bone: {}", imported.Name.c_str());

        BoneReference reference;
        if (!imported.Existing.has_value()) {
            glm::mat4 offset;
            ConvertAssimpMatrix(imported.Pointer->mOffsetMatrix, offset);

            // no-op if node already processed
            ProcessNode(node);
            size_t nodeIndex = m_NodeMap.at(node);

            reference.Index = armature->AddBone(imported.Name, nodeIndex, offset);
        } else {
            reference.Index = imported.Existing.value();
        }

        for (unsigned int i = 0; i < imported.Pointer->mNumWeights; i++) {
            const auto& weight = imported.Pointer->mWeights[i];

            uint32_t index = (uint32_t)weight.mVertexId;
            float value = weight.mWeight;
            FUUJIN_TRACE("Weight for vertex {}: {}", index, value);

            reference.Weights[index] = value;
        }

        bones.push_back(reference);
    }

    static Animation::Behavior ConvertBehavior(aiAnimBehaviour behavior) {
        switch (behavior) {
        case aiAnimBehaviour_DEFAULT:
            return Animation::Behavior::Default;
        case aiAnimBehaviour_CONSTANT:
            return Animation::Behavior::Constant;
        case aiAnimBehaviour_LINEAR:
            return Animation::Behavior::Linear;
        default:
            throw std::runtime_error("Unsupported animation behavior!");
        }
    }

    static Animation::VectorKeyframe ConvertKeyframe(aiAnimation* animation,
                                                     const aiVectorKey& key) {
        ZoneScoped;

        auto time = key.mTime / animation->mTicksPerSecond;
        auto value = glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z);

        Animation::VectorKeyframe keyframe;
        keyframe.Time = Duration((Duration::rep)time);
        keyframe.Value = value;

        return keyframe;
    }

    static Animation::QuaternionKeyframe ConvertKeyframe(aiAnimation* animation,
                                                         const aiQuatKey& key) {
        ZoneScoped;

        auto time = key.mTime / animation->mTicksPerSecond;
        auto value = glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z);

        Animation::QuaternionKeyframe keyframe;
        keyframe.Time = Duration((Duration::rep)time);
        keyframe.Value = value;

        return keyframe;
    }

    template <typename _Src, typename _Dst>
    static void ConvertKeyframes(aiAnimation* animation, unsigned int count, const _Src* data,
                                 std::vector<Animation::Keyframe<_Dst>>& result) {
        ZoneScoped;

        for (unsigned int i = 0; i < count; i++) {
            const auto& key = data[i];
            result.push_back(ConvertKeyframe(animation, key));
        }
    }

    void ModelImporter::ProcessAnimation(aiAnimation* animation) {
        ZoneScoped;

        fs::path filename;
        if (animation->mName.Empty()) {
            filename = m_Model->GetPath().filename().replace_extension(".anim");
        } else {
            filename = std::string(animation->mName.C_Str()) + ".anim";
        }

        fs::path realPath = m_AnimationDirectory / filename;
        fs::path virtualPath = AssetManager::NormalizePath(m_AnimationPrefix / filename);

        double length = animation->mDuration / animation->mTicksPerSecond;
        auto duration = Duration((Duration::rep)length);

        std::vector<Animation::Channel> channels;
        for (unsigned int i = 0; i < animation->mNumChannels; i++) {
            auto channelData = animation->mChannels[i];
            auto& channel = channels.emplace_back();

            channel.Name = channelData->mNodeName.C_Str();
            channel.PreBehavior = ConvertBehavior(channelData->mPreState);
            channel.PostBehavior = ConvertBehavior(channelData->mPostState);

            ConvertKeyframes(animation, channelData->mNumPositionKeys, channelData->mPositionKeys,
                             channel.TranslationKeys);

            ConvertKeyframes(animation, channelData->mNumRotationKeys, channelData->mRotationKeys,
                             channel.RotationKeys);

            ConvertKeyframes(animation, channelData->mNumScalingKeys, channelData->mScalingKeys,
                             channel.ScaleKeys);
        }

        auto result = Ref<Animation>::Create(realPath, duration, channels);
        if (!AssetManager::AddAsset(result, virtualPath)) {
            FUUJIN_WARN("Failed to add animation {} to assets - links may be broken");
        }
    }

    Ref<Material> ModelImporter::GetMaterial(unsigned int index) {
        ZoneScoped;

        if (m_Materials.contains(index)) {
            return m_Materials.at(index);
        }

        auto source = m_Scene->mMaterials[index];
        auto name = source->GetName();
        FUUJIN_INFO("Processing material: {}", name.C_Str());

        fs::path filename = std::string(name.C_Str()) + ".mat";
        fs::path realPath = m_MaterialDirectory / filename;
        fs::path virtualPath = m_MaterialPrefix / filename;
        auto material = Ref<Material>::Create(realPath);

        aiColor4D color;
        float scalar;

        if (source->Get(AI_MATKEY_COLOR_DIFFUSE, color) == aiReturn_SUCCESS) {
            material->SetProperty(Material::Property::AlbedoColor, color);
            FUUJIN_TRACE("Albedo color: <{}, {}, {}, {}>", color.r, color.g, color.b, color.a);
        }

        if (source->Get(AI_MATKEY_COLOR_SPECULAR, color) == aiReturn_SUCCESS) {
            material->SetProperty(Material::Property::SpecularColor, ConvertAssimpColor(color));
            FUUJIN_TRACE("Specular color: <{}, {}, {}, {}>", color.r, color.g, color.b, color.a);
        }

        if (source->Get(AI_MATKEY_COLOR_AMBIENT, color) == aiReturn_SUCCESS) {
            material->SetProperty(Material::Property::AmbientColor, ConvertAssimpColor(color));
            FUUJIN_TRACE("Ambient color: <{}, {}, {}, {}>", color.r, color.g, color.b, color.a);
        }

        if (source->Get(AI_MATKEY_SHININESS, scalar) == aiReturn_SUCCESS) {
            material->SetProperty(Material::Property::Shininess, scalar);
            FUUJIN_TRACE("Shininess: {}", scalar);
        }

        static const std::unordered_map<aiTextureType, Material::TextureSlot> slotMap = {
            { aiTextureType_DIFFUSE, Material::TextureSlot::Albedo },
            { aiTextureType_SPECULAR, Material::TextureSlot::Specular },
            { aiTextureType_AMBIENT, Material::TextureSlot::Ambient },
            { aiTextureType_NORMALS, Material::TextureSlot::Normal },
        };

        for (const auto& [type, slot] : slotMap) {
            if (source->GetTextureCount(type) == 0) {
                continue;
            }

            aiString stringPath;
            if (source->GetTexture(type, 0, &stringPath) != aiReturn_SUCCESS) {
                continue;
            }

            fs::path realTexturePath = stringPath.C_Str();
            if (realTexturePath.is_relative()) {
                realTexturePath = m_ModelDirectory / realTexturePath;
            }

            realTexturePath = realTexturePath.lexically_normal();
            auto virtualTexturePath = AssetManager::GetVirtualPath(realTexturePath);

            Ref<Texture> texture;
            if (virtualTexturePath.has_value()) {
                texture = AssetManager::GetAsset<Texture>(virtualTexturePath.value());
            }

            auto texturePathText = realTexturePath.string();
            if (texture.IsEmpty()) {
                FUUJIN_ERROR("Failed to find texture of path {} - skipping",
                             texturePathText.c_str());

                continue;
            }

            material->SetTexture(slot, texture);
        }

        if (!AssetManager::AddAsset(material, virtualPath)) {
            FUUJIN_WARN("Failed to add material to asset registry - this will create broken links");
        }

        m_Materials[index] = material;
        return material;
    }

    size_t ModelImporter::GetArmature(aiNode* node) {
        ZoneScoped;

        if (m_ArmatureMap.contains(node)) {
            return m_ArmatureMap.at(node);
        }

        // no-op if already inside node or already processed
        ProcessNode(node);

        std::string name = node->mName.C_Str();
        size_t nodeIndex = m_NodeMap.at(node);

        auto armature = std::make_unique<Armature>(name, nodeIndex);
        FUUJIN_DEBUG("Creating new armature: {}", name.c_str());

        size_t index = m_Model->GetArmatures().size();
        m_Model->AddArmature(std::move(armature));

        return index;
    }
} // namespace fuujin