#include "fuujinpch.h"
#include "fuujin/renderer/Model.h"

#include "fuujin/asset/AssetManager.h"

#include "fuujin/renderer/Renderer.h"

#include <assimp/scene.h>

#include <fstream>

namespace fuujin {
    static uint64_t s_MeshID = 0;

    static const std::vector<std::string> s_ModelExtensions = { "model" };

    Mesh::Mesh(const Ref<Material>& material, const std::vector<Vertex>& vertices,
               const std::vector<uint32_t>& indices, const std::vector<BoneReference>& bones,
               size_t armature) {
        ZoneScoped;

        if (material.IsEmpty()) {
            throw std::runtime_error("No material passed!");
        }

        if (vertices.empty()) {
            throw std::runtime_error("No vertices passed!");
        }

        if (indices.empty()) {
            throw std::runtime_error("No indices passed!");
        }

        if (indices.size() % IndicesPerFace != 0) {
            throw std::runtime_error("Mesh not triangulated!");
        }

        m_ID = s_MeshID++;

        m_Material = material;
        m_Vertices = vertices;
        m_Indices = indices;
        m_Bones = bones;
        m_ArmatureIndex = armature;
    }

    Mesh::~Mesh() {
        ZoneScoped;

        Renderer::FreeMesh(m_ID);
    }

    Armature::Armature(const std::string& name, size_t node) {
        ZoneScoped;

        m_Name = name;
        m_Node = node;
    }

    size_t Armature::AddBone(const std::string& name, size_t node, const glm::mat4& offset) {
        ZoneScoped;
        FUUJIN_DEBUG("Adding bone {} to armature {}", name.c_str(), m_Name.c_str());

        if (m_BoneMap.contains(name)) {
            throw std::runtime_error("Bone named " + name + " already exists on this armature!");
        }

        size_t index = m_Bones.size();
        auto& bone = m_Bones.emplace_back();

        bone.Name = name;
        bone.Node = node;
        bone.Offset = offset;

        m_BoneMap[name] = index;
        m_NodeBoneMap[node] = index;

        return index;
    }

    std::optional<size_t> Armature::FindBoneByName(const std::string& name) const {
        ZoneScoped;

        if (m_BoneMap.contains(name)) {
            return m_BoneMap.at(name);
        }

        return {};
    }

    std::optional<size_t> Armature::FindBoneByNode(size_t node) const {
        ZoneScoped;

        if (m_NodeBoneMap.contains(node)) {
            return m_NodeBoneMap.at(node);
        }

        return {};
    }

    Model::Model(const fs::path& path) {
        ZoneScoped;

        m_Path = path;
    }

    void Model::AddMesh(std::unique_ptr<Mesh>&& mesh) {
        ZoneScoped;

        m_Meshes.push_back(std::move(mesh));
    }

    void Model::AddArmature(std::unique_ptr<Armature>&& armature) {
        ZoneScoped;

        m_Armatures.push_back(std::move(armature));
    }

    size_t Model::AddNode(const std::string& name, const glm::mat4& transform,
                          const std::unordered_set<size_t>& meshes,
                          const std::optional<size_t>& parent) {
        ZoneScoped;

        if (m_NodeMap.contains(name)) {
            throw std::runtime_error("Duplicate node: " + name);
        }

        size_t index = m_Nodes.size();

        auto& node = m_Nodes.emplace_back();
        node.Name = name;
        node.Transform = transform;
        node.Meshes = meshes;
        node.Parent = parent;

        if (parent.has_value()) {
            size_t parentIndex = parent.value();
            m_Nodes[parentIndex].Children.insert(index);
        }

        m_NodeMap[name] = index;
        if (!meshes.empty()) {
            m_MeshNodes.insert(index);
        }

        return index;
    }

    std::optional<size_t> Model::FindNode(const std::string& name) {
        ZoneScoped;

        if (m_NodeMap.contains(name)) {
            return m_NodeMap.at(name);
        }

        return {};
    }

    static std::unique_ptr<Mesh> DeserializeMesh(const YAML::Node& node) {
        ZoneScoped;

        const auto& verticesNode = node["Vertices"];
        if (!verticesNode.IsDefined()) {
            FUUJIN_ERROR("Mesh has no vertices!");
            return nullptr;
        }

        const auto& facesNode = node["Faces"];
        if (!facesNode.IsDefined()) {
            FUUJIN_ERROR("Mesh has no faces!");
            return nullptr;
        }

        const auto& materialNode = node["Material"];
        if (!materialNode.IsDefined()) {
            FUUJIN_ERROR("Mesh has no material!");
            return nullptr;
        }

        std::vector<Vertex> vertices;
        for (const auto& vertexNode : verticesNode) {
            Vertex vertex{};

            const auto& positionNode = vertexNode["Position"];
            if (!positionNode.IsDefined()) {
                FUUJIN_ERROR("Vertex has no position - aborting mesh");
                return nullptr;
            }

            const auto& uvNode = vertexNode["UV"];
            const auto& normalNode = vertexNode["Normal"];
            const auto& tangentNode = vertexNode["Tangent"];

            vertex.Position = positionNode.as<glm::vec3>();
            if (uvNode.IsDefined()) {
                vertex.UV = uvNode.as<glm::vec2>();
            }

            if (normalNode.IsDefined()) {
                vertex.Normal = normalNode.as<glm::vec3>();
            }

            if (tangentNode.IsDefined()) {
                vertex.Tangent = tangentNode.as<glm::vec3>();
            }

            vertices.push_back(vertex);
        }

        std::vector<uint32_t> indices;
        for (const auto& faceNode : facesNode) {
            if (!faceNode.IsSequence() || faceNode.size() != Mesh::IndicesPerFace) {
                FUUJIN_ERROR("Face not triangulated - skipping mesh");
                return nullptr;
            }

            for (const auto& indexNode : faceNode) {
                indices.push_back(indexNode.as<uint32_t>());
            }
        }

        fs::path virtualMaterialPath = materialNode.as<std::string>();
        auto material = AssetManager::GetAsset<Material>(virtualMaterialPath);

        if (!material) {
            auto virtualText = virtualMaterialPath.string();
            FUUJIN_ERROR("Failed to find material: {}", virtualText.c_str());

            return nullptr;
        }

        const auto& armatureNode = node["Armature"];
        const auto& bonesNode = node["Bones"];

        std::vector<BoneReference> bones;
        size_t armature = 0;

        if (armatureNode.IsDefined() && bonesNode.IsDefined()) {
            armature = armatureNode.as<size_t>();

            for (const auto& boneNode : bonesNode) {
                BoneReference bone;
                bone.Index = boneNode["Index"].as<size_t>();

                const auto& weightsNode = boneNode["Weights"];
                for (const auto& weightNode : weightsNode) {
                    uint32_t vertex = weightNode["Vertex"].as<uint32_t>();
                    float weight = weightNode["Weight"].as<float>();

                    bone.Weights[vertex] = weight;
                }

                bones.push_back(bone);
            }
        } else {
            FUUJIN_DEBUG("Either armature or bone references weren't specified - skipping rigging");
        }

        return std::make_unique<Mesh>(material, vertices, indices, bones, armature);
    }

    static std::unique_ptr<Armature> DeserializeArmature(const YAML::Node& node) {
        ZoneScoped;

        auto name = node["Name"].as<std::string>();
        auto nodeIndex = node["Node"].as<size_t>();
        auto armature = std::make_unique<Armature>(name, nodeIndex);

        const auto& bonesNode = node["Bones"];
        for (const auto& boneNode : bonesNode) {
            std::optional<size_t> parent;

            auto boneName = boneNode["Name"].as<std::string>();
            auto boneNodeIndex = boneNode["Node"].as<size_t>();
            auto offset = boneNode["Offset"].as<glm::mat4>();

            armature->AddBone(boneName, boneNodeIndex, offset);
        }

        return armature;
    }

    Ref<Asset> ModelSerializer::Deserialize(const fs::path& path) const {
        ZoneScoped;

        std::ifstream file(path);

        auto pathText = path.string();
        if (!file.is_open()) {
            FUUJIN_ERROR("Failed to open path: {}", pathText.c_str());
            return nullptr;
        }

        FUUJIN_INFO("Loading model from path: {}", pathText.c_str());
        auto node = YAML::Load(file);
        file.close();

        auto model = Ref<Model>::Create(path);

        const auto& nodesNode = node["Nodes"];
        for (const auto& nodeNode : nodesNode) {
            auto name = nodeNode["Name"].as<std::string>();
            auto transform = nodeNode["Transform"].as<glm::mat4>();

            std::unordered_set<size_t> meshes;
            const auto& meshIndicesNode = nodeNode["Meshes"];
            if (meshIndicesNode.IsDefined()) {
                for (const auto& meshIndexNode : meshIndicesNode) {
                    meshes.insert(meshIndexNode.as<size_t>());
                }
            }

            const auto& parentNode = nodeNode["Parent"];
            std::optional<size_t> parent;
            if (parentNode.IsDefined()) {
                parent = parentNode.as<size_t>();
            }

            model->AddNode(name, transform, meshes, parent);
        }

        const auto& meshesNode = node["Meshes"];
        if (meshesNode.IsDefined()) {
            for (const auto& meshNode : meshesNode) {
                auto mesh = DeserializeMesh(meshNode);
                if (!mesh) {
                    FUUJIN_WARN("Failed to load mesh - skipping");
                    continue;
                }

                model->AddMesh(std::move(mesh));
            }
        }

        if (model->GetMeshes().empty()) {
            FUUJIN_ERROR("Model has no meshes!");
            return nullptr;
        }

        const auto& armaturesNode = node["Armatures"];
        if (armaturesNode.IsDefined()) {
            for (const auto& armatureNode : armaturesNode) {
                auto armature = DeserializeArmature(armatureNode);

                model->AddArmature(std::move(armature));
            }
        }

        return model;
    }

    static bool SerializeMesh(const std::unique_ptr<Mesh>& mesh, YAML::Node& node) {
        ZoneScoped;

        YAML::Node verticesNode;
        const auto& vertices = mesh->GetVertices();
        for (const auto& vertex : vertices) {
            YAML::Node vertexNode;

            vertexNode["Position"] = vertex.Position;
            vertexNode["UV"] = vertex.UV;
            vertexNode["Normal"] = vertex.Normal;
            vertexNode["Tangent"] = vertex.Tangent;

            verticesNode.push_back(vertexNode);
        }

        YAML::Node facesNode;
        const auto& indices = mesh->GetIndices();

        for (size_t i = 0; i < indices.size(); i += Mesh::IndicesPerFace) {
            YAML::Node faceNode;
            faceNode.SetStyle(YAML::EmitterStyle::Flow);

            for (size_t j = 0; j < Mesh::IndicesPerFace; j++) {
                faceNode.push_back(indices[i + j]);
            }

            facesNode.push_back(faceNode);
        }

        auto material = mesh->GetMaterial();
        auto virtualPath = AssetManager::GetVirtualPath(material->GetPath());

        if (!virtualPath.has_value()) {
            FUUJIN_ERROR("Material is not registered! Skipping mesh");
            return false;
        }

        node["Material"] = virtualPath.value().string();
        node["Vertices"] = verticesNode;
        node["Faces"] = facesNode;

        const auto& bones = mesh->GetBones();
        if (!bones.empty()) {
            YAML::Node bonesNode;
            for (const auto& bone : bones) {
                YAML::Node weightsNode;
                for (const auto& [vertex, weight] : bone.Weights) {
                    YAML::Node weightNode;
                    weightNode["Vertex"] = vertex;
                    weightNode["Weight"] = weight;

                    weightsNode.push_back(weightNode);
                }

                YAML::Node boneNode;
                boneNode["Index"] = bone.Index;
                boneNode["Weights"] = weightsNode;

                bonesNode.push_back(boneNode);
            }

            node["Bones"] = bonesNode;
            node["Armature"] = mesh->GetArmatureIndex();
        }

        return true;
    }

    static bool SerializeArmature(const std::unique_ptr<Armature>& armature, YAML::Node& node) {
        ZoneScoped;

        node["Name"] = armature->GetName();
        node["Node"] = armature->GetNode();

        const auto& bones = armature->GetBones();
        YAML::Node bonesNode;

        for (const auto& bone : bones) {
            YAML::Node boneNode;

            boneNode["Name"] = bone.Name;
            boneNode["Node"] = bone.Node;
            boneNode["Offset"] = bone.Offset;

            bonesNode.push_back(boneNode);
        }

        node["Bones"] = bonesNode;

        return true;
    }

    bool ModelSerializer::Serialize(const Ref<Asset>& asset) const {
        ZoneScoped;

        if (asset->GetAssetType() != AssetType::Model) {
            return false;
        }

        auto model = asset.As<Model>();
        const auto& meshes = model->GetMeshes();
        const auto& armatures = model->GetArmatures();
        const auto& nodes = model->GetNodes();

        YAML::Node meshesNode;
        size_t skippedMeshes = 0;
        for (const auto& mesh : meshes) {
            YAML::Node meshNode;
            if (!SerializeMesh(mesh, meshNode)) {
                FUUJIN_WARN("Failed to serialize mesh! Skipping");

                skippedMeshes++;
                continue;
            }

            meshesNode.push_back(meshNode);
        }

        if (skippedMeshes == meshes.size()) {
            FUUJIN_ERROR("All meshes skipped due to error - aborting model serialization");
            return false;
        }

        YAML::Node nodesNode;
        for (const auto& nodeData : nodes) {
            YAML::Node meshesNode;
            for (size_t meshIndex : nodeData.Meshes) {
                meshesNode.push_back(meshIndex);
            }

            YAML::Node nodeNode;
            nodeNode["Name"] = nodeData.Name;
            nodeNode["Transform"] = nodeData.Transform;
            nodeNode["Meshes"] = meshesNode;

            if (nodeData.Parent.has_value()) {
                nodeNode["Parent"] = nodeData.Parent.value();
            }

            nodesNode.push_back(nodeNode);
        }

        YAML::Node armaturesNode;
        size_t skippedArmatures = 0;
        for (const auto& armature : armatures) {
            YAML::Node armatureNode;
            if (!SerializeArmature(armature, armatureNode)) {
                FUUJIN_WARN("Failed to serialize armature! Skipping");

                skippedArmatures++;
                continue;
            }

            armaturesNode.push_back(armatureNode);
        }

        YAML::Node node;
        node["Meshes"] = meshesNode;
        node["Nodes"] = nodesNode;

        if (skippedArmatures != armatures.size()) {
            node["Armatures"] = armaturesNode;
        }

        const auto& path = model->GetPath();
        if (path.has_parent_path()) {
            auto directory = path.parent_path();
            fs::create_directories(directory);
        }

        std::ofstream file(path);

        auto pathText = path.string();
        if (!file.is_open()) {
            FUUJIN_ERROR("Failed to open path {}", pathText.c_str());
            return false;
        }

        file << YAML::Dump(node);
        file.close();

        FUUJIN_INFO("Serialized model to path {}", pathText.c_str());
        return true;
    }

    const std::vector<std::string>& ModelSerializer::GetExtensions() const {
        return s_ModelExtensions;
    }
} // namespace fuujin