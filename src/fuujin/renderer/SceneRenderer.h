#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/animation/Animator.h"

#include "fuujin/scene/Scene.h"

namespace fuujin {
    class SceneRenderer : public RefCounted {
    public:
        SceneRenderer(const Ref<Scene>& scene);
        ~SceneRenderer();

        void RenderScene();

    private:
        void RenderSceneWithID(uint64_t id, size_t firstCamera, size_t cameraCount);

        Ref<Scene> m_Scene;
        uint64_t m_MainID;

        std::unordered_map<Scene::Entity, Ref<Animator>> m_Animators;
    };
} // namespace fuujin
