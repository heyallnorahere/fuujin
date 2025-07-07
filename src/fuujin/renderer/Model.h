#pragma once
#include "fuujin/asset/Asset.h"

#include "fuujin/asset/ModelSource.h"

#include "fuujin/renderer/Material.h"

namespace fuujin {
    struct Vertex {
        glm::vec3 Position;
        glm::vec2 UV;
        glm::vec3 Normal, Tangent;
    };

    struct BoneReference {
        size_t Index;
        std::map<uint32_t, float> Weights;
    };

    struct Bone {
        std::optional<size_t> Parent;
        std::string Name;
        glm::mat4 Transform;
        glm::mat4 Offset;
        std::unordered_set<size_t> Children;
    };

    class Mesh {
    public:
        static constexpr uint32_t IndicesPerFace = 3;

        Mesh(const Ref<Material>& material, const std::vector<Vertex>& vertices,
             const std::vector<uint32_t>& indices, const std::vector<BoneReference>& bones,
             size_t armature);

        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        uint64_t GetID() const { return m_ID; }

        const Ref<Material>& GetMaterial() const { return m_Material; }
        const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
        const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

        const std::vector<BoneReference>& GetBones() const { return m_Bones; }
        size_t GetArmatureIndex() const { return m_ArmatureIndex; }

    private:
        uint64_t m_ID;

        Ref<Material> m_Material;
        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;

        std::vector<BoneReference> m_Bones;
        size_t m_ArmatureIndex;
    };

    class Armature {
    public:
        Armature(const std::string& name);
        ~Armature() = default;

        Armature(const Armature&) = delete;
        Armature& operator=(const Armature&) = delete;

        size_t AddBone(const std::optional<size_t>& parent, const std::string& name,
                       const glm::mat4& transform, const glm::mat4& offset);
        
        std::optional<size_t> FindBone(const std::string& name) const;

        uint64_t GetID() { return m_ID; }
        const std::string& GetName() const { return m_Name; }

        const std::vector<Bone>& GetBones() const { return m_Bones; }

    private:
        uint64_t m_ID;
        std::string m_Name;

        std::vector<Bone> m_Bones;
    };

    class Model : public Asset {
    public:
        Model(const fs::path& path);
        virtual ~Model() override = default;

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        virtual const fs::path& GetPath() const override { return m_Path; }
        virtual AssetType GetAssetType() const override { return AssetType::Model; }

        void AddMesh(std::unique_ptr<Mesh>&& mesh);
        void AddArmature(std::unique_ptr<Armature>&& armature);

        const std::vector<std::unique_ptr<Mesh>>& GetMeshes() const { return m_Meshes; }
        const std::vector<std::unique_ptr<Armature>>& GetArmatures() const { return m_Armatures; }

    private:
        fs::path m_Path;

        std::vector<std::unique_ptr<Mesh>> m_Meshes;
        std::vector<std::unique_ptr<Armature>> m_Armatures;
    };

    template <>
    inline std::optional<AssetType> GetAssetType<Model>() {
        return AssetType::Model;
    }

    class ModelSerializer : public AssetSerializer {
    public:
        virtual Ref<Asset> Deserialize(const fs::path& path) const override;
        virtual bool Serialize(const Ref<Asset>& asset) const override;

        virtual const std::vector<std::string>& GetExtensions() const override;
        virtual AssetType GetType() const override { return AssetType::Model; }
    };
} // namespace fuujin