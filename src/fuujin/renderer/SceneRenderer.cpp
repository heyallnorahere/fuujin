#include "fuujinpch.h"
#include "fuujin/renderer/SceneRenderer.h"

#include "fuujin/renderer/Renderer.h"

#include "fuujin/scene/Components.h"

namespace fuujin {
    static uint64_t s_RendererSceneID = 0;
    static constexpr uint32_t s_ShadowResolution = 1024;

    SceneRenderer::SceneRenderer(const Ref<Scene>& scene) {
        ZoneScoped;

        m_Scene = scene;
        m_MainID = s_RendererSceneID++;

        Sampler::Spec shadowSamplerSpec;
        shadowSamplerSpec.U = AddressMode::ClampToBorder;
        shadowSamplerSpec.V = AddressMode::ClampToBorder;
        shadowSamplerSpec.W = AddressMode::ClampToBorder;
        shadowSamplerSpec.Border = BorderColor::FloatOpaqueWhite;

        auto context = Renderer::GetContext();
        m_ShadowSampler = context->CreateSampler(shadowSamplerSpec);
    }

    SceneRenderer::~SceneRenderer() {
        ZoneScoped;

        Renderer::FreeScene(m_MainID);
        for (const auto& [entity, data] : m_LightShadowData) {
            Renderer::FreeScene(data.SceneID);
        }
    }

    void SceneRenderer::RenderScene() {
        ZoneScoped;

//        RenderShadows();
        RenderMainScene();
    }

    static bool AreSpecsEqual(const ShadowFramebufferSpec& lhs, const ShadowFramebufferSpec& rhs) {
        return lhs.AttachmentType == rhs.AttachmentType && lhs.Layers == rhs.Layers &&
               lhs.Resolution == rhs.Resolution;
    }

    void SceneRenderer::RenderShadowMap(Scene::Entity entity, const glm::mat4& transform,
                                        const Ref<Light>& light) {
        ZoneScoped;
        const auto& api = Renderer::GetAPI();

        bool framebufferExists = m_LightShadowData.contains(entity);
        auto& shadowData = m_LightShadowData[entity];

        if (!framebufferExists) {
            shadowData.SceneID = s_RendererSceneID++;
        }

        ShadowFramebufferSpec currentSpec;
        currentSpec.Resolution = s_ShadowResolution;

        Renderer::SceneData sceneData;
        switch (light->GetType()) {
        case Light::Type::Point: {
            static const glm::vec3 up = glm::vec3(0.f, -1.f, 0.f);
            static const std::vector<glm::vec3> cubeFaceDirections = {
                glm::vec3(1.f, 0.f, 0.f),  glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f),
                glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, 1.f),  glm::vec3(0.f, 0.f, -1.f)
            };

            currentSpec.Layers = (uint32_t)cubeFaceDirections.size();
            currentSpec.AttachmentType = Texture::Type::Cube;

            auto pointLight = light.As<PointLight>();
            glm::vec3 offset = pointLight->GetPosition();

            glm::vec2 zRange = pointLight->GetShadowZRange();
            float near = zRange[0];
            float far = zRange[1];

            glm::vec3 position = transform * glm::vec4(offset, 1.f);
            glm::mat4 projection = Camera::Perspective(api, glm::radians(90.f), 1.f, near, far);

            for (const auto& direction : cubeFaceDirections) {
                auto& camera = sceneData.Cameras.emplace_back();
                camera.Position = position;
                camera.ZRange = zRange;

                glm::vec3 faceRight = glm::cross(direction, up);
                glm::vec3 faceUp =
                    glm::normalize(glm::cross(faceRight, direction) +
                                   glm::dot(direction, up) * glm::vec3(0.f, 0.f, -1.f));

                glm::mat4 view = Camera::LookAt(api, position, position + direction, faceUp);
                camera.ViewProjection = projection * view;
            }
        } break;
        default:
            FUUJIN_ERROR("Invalid light type!");
            return;
        }

        Renderer::UpdateScene(shadowData.SceneID, sceneData);
        if (!framebufferExists || !AreSpecsEqual(currentSpec, shadowData.Spec)) {
            shadowData.Spec = currentSpec;

            Framebuffer::Spec spec;
            spec.Layers = currentSpec.Layers;
            spec.Width = currentSpec.Resolution;
            spec.Height = currentSpec.Resolution;

            auto& depthSpec = spec.Attachments.emplace_back();
            depthSpec.TextureSampler = m_ShadowSampler;
            depthSpec.Type = Framebuffer::AttachmentType::Depth;
            depthSpec.ImageType = Texture::Type::Cube;
            depthSpec.Format = Texture::Format::D32;

            auto context = Renderer::GetContext();
            uint32_t frameCount = Renderer::GetFrameCount();
            shadowData.Framebuffers = context->CreateFramebuffers(spec, frameCount);
        }

        uint32_t currentFrame = Renderer::GetCurrentFrame();
        auto framebuffer = shadowData.Framebuffers[currentFrame];

        Renderer::PushRenderTarget(framebuffer);
        RenderSceneWithID(shadowData.SceneID, 0, sceneData.Cameras.size());
        Renderer::PopRenderTarget();
    }

    void SceneRenderer::RenderShadows() {
        ZoneScoped;

        m_Scene->View<LightComponent>([&](Scene::Entity entity, LightComponent& light) {
            glm::mat4 transformMatrix(1.f);
            if (entity.HasAll<TransformComponent>()) {
                const auto& transform = entity.GetComponent<TransformComponent>();
                transformMatrix = transform.Data.ToMatrix();
            }

            RenderShadowMap(entity, transformMatrix, light.SceneLight);
        });
    }

    void SceneRenderer::RenderMainScene() {
        ZoneScoped;

        auto mainTarget = Renderer::GetActiveRenderTarget();
        if (mainTarget.IsEmpty()) {
            FUUJIN_ERROR("Attempted to render scene with no render target!");
            return;
        }

        uint32_t cameraWidth = mainTarget->GetWidth();
        uint32_t cameraHeight = mainTarget->GetHeight();

        Renderer::SceneData mainScene;
        size_t mainCamera = 0;
        m_Scene->View<CameraComponent>([&](Scene::Entity entity, CameraComponent& camera) {
            if (camera.MainCamera) {
                mainCamera = mainScene.Cameras.size();
            }

            glm::mat4 transformMatrix(1.f);
            if (entity.HasAll<TransformComponent>()) {
                const auto& transform = entity.GetComponent<TransformComponent>();
                transformMatrix = transform.Data.ToMatrix();
            }

            auto& entityCamera = camera.CameraInstance;
            entityCamera.SetViewSize({ cameraWidth, cameraHeight });
            auto viewProjection = entityCamera.CalculateViewProjection(transformMatrix);

            auto& rendererCamera = mainScene.Cameras.emplace_back();
            rendererCamera.Position = glm::vec3(transformMatrix * glm::vec4(glm::vec3(0.f), 1.f));
            rendererCamera.ZRange = entityCamera.GetZRange();
            rendererCamera.ViewProjection = viewProjection;
        });

        m_Scene->View<LightComponent>([&](Scene::Entity entity, LightComponent& light) {
            if (light.SceneLight.IsEmpty()) {
                return;
            }

            auto& rendererLight = mainScene.Lights.emplace_back();
            rendererLight.LightData = light.SceneLight;

            if (entity.HasAll<TransformComponent>()) {
                const auto& transform = entity.GetComponent<TransformComponent>();
                rendererLight.TransformMatrix = transform.Data.ToMatrix();
            } else {
                rendererLight.TransformMatrix = glm::mat4(1.f);
            }
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
