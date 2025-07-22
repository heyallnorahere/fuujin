#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/Model.h"

namespace fuujin {
    class Animator : public RefCounted {
    public:
        Animator(const Ref<Model>& model);
        ~Animator();

        Animator(const Animator&) = delete;
        Animator& operator=(const Animator&) = delete;

        void Update();

        void SetLocalTransform(size_t node, const glm::mat4& matrix);
        void ResetLocalTransform(size_t node);

        const std::vector<glm::mat4>& GetNodeTransforms() const { return m_NodeTransforms; }
        const std::vector<glm::mat4>& GetBoneTransforms() const { return m_BoneTransforms; }
        const std::vector<size_t>& GetArmatureOffsets() const { return m_ArmatureOffsets; }

        uint64_t GetID() const { return m_ID; }
        uint64_t GetState() const { return m_State; }

        const Ref<Model>& GetModel() const { return m_Model; }

    private:
        void WalkTree(size_t nodeIndex, const glm::mat4& parentTransform);

        Ref<Model> m_Model;

        std::map<size_t, glm::mat4> m_LocalTransforms;
        std::vector<glm::mat4> m_NodeTransforms, m_BoneTransforms;
        std::vector<size_t> m_ArmatureOffsets;

        bool m_Updated;
        uint64_t m_ID;
        uint64_t m_State;
    };
} // namespace fuujin
