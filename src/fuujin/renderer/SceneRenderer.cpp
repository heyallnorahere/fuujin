#include "fuujinpch.h"
#include "fuujin/renderer/SceneRenderer.h"

#include "fuujin/renderer/Renderer.h"

#include "fuujin/scene/Components.h"

namespace fuujin {
    static uint64_t s_RendererSceneID = 0;

    SceneRenderer::SceneRenderer(const Ref<Scene>& scene) {
        ZoneScoped;

        m_Scene = scene;
        m_MainID = s_RendererSceneID++;
    }

    SceneRenderer::~SceneRenderer() {
        ZoneScoped;

        Renderer::FreeScene(m_MainID);
    }

    void SceneRenderer::RenderScene() {
        ZoneScoped;

        auto mainTarget = Renderer::GetActiveRenderTarget();
        if (mainTarget.IsEmpty()) {
            FUUJIN_ERROR("Attempted to render scene with no render target!");
            return;
        }

        // todo: lighting passes

        uint32_t cameraWidth = mainTarget->GetWidth();
        uint32_t cameraHeight = mainTarget->GetHeight();

        Renderer::SceneData mainScene;
        size_t mainCamera = 0;
        m_Scene->View<TransformComponent, CameraComponent>(
            [&](Scene::Entity entity, TransformComponent& transform, CameraComponent& camera) {
                if (camera.MainCamera) {
                    mainCamera = mainScene.Cameras.size();
                }

                const auto& translation = transform.Data.GetTranslation();
                const auto& rotation = transform.Data.GetRotationQuat();

                auto& entityCamera = camera.CameraInstance;
                entityCamera.SetViewSize({ cameraWidth, cameraHeight });
                auto viewProjection = entityCamera.CalculateViewProjection(translation, rotation);

                auto& rendererCamera = mainScene.Cameras.emplace_back();
                rendererCamera.Position = translation;
                rendererCamera.ViewProjection = viewProjection;
            });

        m_Scene->View<TransformComponent, LightComponent>(
            [&](Scene::Entity entity, TransformComponent& transform, LightComponent& light) {
                if (light.SceneLight.IsEmpty()) {
                    return;
                }

                auto& rendererLight = mainScene.Lights.emplace_back();
                rendererLight.LightData = light.SceneLight;
                rendererLight.TransformMatrix = transform.Data.ToMatrix();
            });

        if (!mainScene.Cameras.empty()) {
            Renderer::UpdateScene(m_MainID, mainScene);
            RenderSceneWithID(m_MainID, mainCamera, 1);
        }
    }

    void SceneRenderer::RenderSceneWithID(uint64_t id, size_t firstCamera, size_t cameraCount) {
        ZoneScoped;

        m_Scene->View<TransformComponent, ModelComponent>(
            [&](Scene::Entity entity, TransformComponent& transform, ModelComponent& model) {
                glm::mat4 modelMatrix = transform.Data.ToMatrix();

                if (!m_Animators.contains(entity) ||
                    m_Animators.at(entity)->GetModel() != model.RenderedModel) {
                    m_Animators[entity] = Ref<Animator>::Create(model.RenderedModel);
                }

                auto animator = m_Animators.at(entity);
                // todo: animation components

                ModelRenderCall call;
                call.RenderedModel = model.RenderedModel;
                call.ModelAnimator = animator;
                call.ModelMatrix = modelMatrix;
                call.FirstCamera = firstCamera;
                call.CameraCount = cameraCount;
                call.SceneID = id;

                Renderer::RenderModel(call);
            });
    }
} // namespace fuujin
