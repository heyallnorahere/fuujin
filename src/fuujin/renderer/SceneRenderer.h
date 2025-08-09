#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/animation/Animator.h"

#include "fuujin/scene/Scene.h"

#include "fuujin/renderer/Light.h"
#include "fuujin/renderer/Framebuffer.h"

namespace fuujin {
    struct ShadowFramebufferSpec {
        uint32_t Resolution;
        uint32_t Layers;
        Texture::Type AttachmentType;
    };

    struct LightShadowData {
        std::vector<Ref<Framebuffer>> Framebuffers;
        ShadowFramebufferSpec Spec;
        uint64_t SceneID;
    };

    class SceneRenderer : public RefCounted {
    public:
        SceneRenderer(const Ref<Scene>& scene);
        ~SceneRenderer();

        void RenderScene();

    private:
        void RenderShadowMap(Scene::Entity entity, const glm::mat4& transform,
                             const Ref<Light>& light);

        void RenderShadows();
        void RenderMainScene();

        void RenderSceneWithID(uint64_t id, size_t firstCamera, size_t cameraCount);

        Ref<Scene> m_Scene;
        uint64_t m_MainID;

        std::unordered_map<Scene::Entity, Ref<Animator>> m_Animators;
        std::unordered_map<Scene::Entity, LightShadowData> m_LightShadowData;

        Ref<Sampler> m_ShadowSampler;
    };
} // namespace fuujin
