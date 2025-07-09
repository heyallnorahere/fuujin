#include "fuujinpch.h"
#include "fuujin/animation/Animator.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    static uint64_t s_AnimatorID = 0;

    Animator::Animator(const Ref<Model>& model) {
        ZoneScoped;

        m_Model = model;
        m_Updated = true;

        m_ID = s_AnimatorID++;
        m_State = 0;
    }

    Animator::~Animator() {
        ZoneScoped;

        Renderer::FreeAnimator(m_ID);
    }

    void Animator::Update() {
        ZoneScoped;

        if (!m_Updated) {
            FUUJIN_TRACE("Animator not updated - skipping update");
            return;
        }

        const auto& armatures = m_Model->GetArmatures();
        const auto& nodes = m_Model->GetNodes();

        if (nodes.empty()) {
            FUUJIN_WARN("No nodes in model - skipping animator update");
            return;
        }

        if (m_ArmatureOffsets.size() != armatures.size()) {
            m_ArmatureOffsets.resize(armatures.size());
        }

        if (m_NodeTransforms.size() != nodes.size()) {
            m_NodeTransforms.resize(nodes.size(), glm::mat4(1.f));
        }

        size_t totalBones = 0;
        for (size_t i = 0; i < armatures.size(); i++) {
            const auto& armature = armatures[i];

            m_ArmatureOffsets[i] = totalBones;
            totalBones += armature->GetBones().size();
        }

        if (m_BoneTransforms.size() != totalBones) {
            m_BoneTransforms.resize(totalBones, glm::mat4(1.f));
        }

        WalkTree(0, glm::inverse(nodes[0].Transform));

        m_State++;
        m_Updated = false;
    }

    void Animator::SetLocalTransform(size_t node, const glm::mat4& matrix) {
        ZoneScoped;

        m_LocalTransforms[node] = matrix;
        m_Updated = true;
    }

    void Animator::ResetLocalTransform(size_t node) {
        ZoneScoped;

        m_LocalTransforms.erase(node);
        m_Updated = true;
    }

    void Animator::WalkTree(size_t nodeIndex, const glm::mat4& parentTransform) {
        ZoneScoped;

        const auto& nodes = m_Model->GetNodes();
        const auto& node = nodes[nodeIndex];

        glm::mat4 localTransform;
        if (m_LocalTransforms.contains(nodeIndex)) {
            localTransform = m_LocalTransforms.at(nodeIndex);
        } else {
            localTransform = node.Transform;
        }

        glm::mat4 nodeTransform = parentTransform * localTransform;
        const auto& armatures = m_Model->GetArmatures();
        for (size_t i = 0; i < armatures.size(); i++) {
            const auto& armature = armatures[i];
            auto boneIndex = armature->FindBoneByNode(nodeIndex);

            if (boneIndex.has_value()) {
                size_t boneID = boneIndex.value();

                const auto& bones = armature->GetBones();
                const auto& bone = bones[boneID];

                size_t boneOffset = m_ArmatureOffsets[i];
                m_BoneTransforms[boneOffset + boneID] = nodeTransform * bone.Offset;
            }
        }

        for (size_t childIndex : node.Children) {
            WalkTree(childIndex, nodeTransform);
        }
    }
} // namespace fuujin