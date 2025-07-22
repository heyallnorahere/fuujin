#include "fuujin.h"

#include "fuujin/asset/AssetManager.h"
#include "fuujin/asset/ModelImporter.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/Model.h"
#include "fuujin/renderer/SceneRenderer.h"

#include "fuujin/imgui/ImGuiLayer.h"

#include "fuujin/scene/Components.h"
#include "fuujin/scene/Scene.h"

#include <numbers>

#include <imgui.h>

using namespace std::chrono_literals;
using namespace fuujin;

struct DemoLight {
    Scene::Entity Entity;
    float Angle;
    glm::vec3 Axis, Coaxis;
};

static const float s_PI = std::numbers::pi_v<float>;
class TestLayer : public Layer {
public:
    TestLayer() {
        ZoneScoped;

        m_Time = 0.f;
        m_AnimationTime = Duration(0);
        m_ResourcesLoaded = false;
    }

    virtual void Update(Duration delta) override {
        ZoneScoped;
        m_Time += delta.count();

        if (!m_ResourcesLoaded) {
            LoadResources();
            m_ResourcesLoaded = true;
        }

        float yaw = m_Time * s_PI;
        float sinYaw = glm::sin(yaw);
        float cosYaw = glm::cos(yaw);

        float pitch = sinYaw * s_PI / 8.f;
        float sinPitch = glm::sin(pitch);
        float cosPitch = glm::cos(pitch);

        glm::vec3 radial = glm::vec3(sinYaw * cosPitch, -sinPitch, cosYaw * cosPitch);
        static constexpr float cameraDistance = 5.f;

        auto pitchMat = glm::rotate(glm::mat4(1.f), pitch, glm::vec3(1.f, 0.f, 0.f));
        auto yawMat = glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0.f, 1.f, 0.f));

        auto& cameraTransform = m_Camera.GetComponent<TransformComponent>().Data;
        cameraTransform.SetTranslation(radial * cameraDistance);
        cameraTransform.SetRotation(glm::toQuat(yawMat * pitchMat));

        for (auto& light : m_Lights) {
            light.Angle += (float)delta.count() * s_PI * 3.f;

            float sin = glm::sin(light.Angle);
            float cos = glm::cos(light.Angle);

            glm::vec3 tertiaryAxis = glm::cross(light.Axis, light.Coaxis);
            glm::vec3 radialLightAxis = cos * light.Coaxis + sin * tertiaryAxis;

            auto& lightTransform = light.Entity.GetComponent<TransformComponent>().Data;
            lightTransform.SetTranslation(radialLightAxis * 2.f);
        }

        m_Renderer->RenderScene();

        static bool demoOpen = true;
        if (demoOpen) {
            ImGui::ShowDemoWindow(&demoOpen);
        }
    }

private:
    void LoadResources() {
        ZoneScoped;

        static const std::string modelName = "Gunman.model";
        static const std::string sourceName = "Gunman.gltf";

        static const fs::path modelDirectory = "fuujin/models";
        static const fs::path modelPath = modelDirectory / modelName;
        static const fs::path sourcePath = modelDirectory / sourceName;

        if (!AssetManager::AssetExists(modelPath)) {
            auto source = AssetManager::GetAsset<ModelSource>(sourcePath);
            if (source.IsEmpty()) {
                throw std::runtime_error("Failed to find model source to import!");
            }

            ModelImporter importer;
            if (importer.Import(source).value_or("") != modelPath) {
                throw std::runtime_error("Failed to import model!");
            }
        }

        auto model = AssetManager::GetAsset<Model>(modelPath);

        m_Scene = Ref<Scene>::Create();
        m_Renderer = Ref<SceneRenderer>::Create(m_Scene);

        auto cube = m_Scene->Create("Cube");
        cube.AddComponent<TransformComponent>().Data.SetScale(glm::vec3(0.01f));
        cube.AddComponent<ModelComponent>().RenderedModel = model;

        m_Camera = m_Scene->Create("Camera");
        m_Camera.AddComponent<TransformComponent>();
        m_Camera.AddComponent<CameraComponent>().MainCamera = true;

        static constexpr size_t lightCount = 3;
        for (size_t i = 0; i < lightCount; i++) {
            glm::vec3 axis(0.f);
            axis[i] = 1.f;

            glm::vec3 coaxis(0.f);
            coaxis[(i + 1) % 3] = 1.f;

            auto pointLight = Ref<PointLight>::Create();
            pointLight->SetColor(Light::Color::Diffuse, axis);

            auto& light = m_Lights.emplace_back();
            light.Axis = axis;
            light.Coaxis = coaxis;
            light.Angle = 2.f * s_PI * (float)i / (float)lightCount;

            light.Entity = m_Scene->Create("Entity #" + std::to_string(i + 1));
            light.Entity.AddComponent<TransformComponent>();
            light.Entity.AddComponent<LightComponent>().SceneLight = pointLight;
        }

        Renderer::Wait();
    }

    float m_Time;
    Duration m_AnimationTime;

    bool m_ResourcesLoaded;
    Ref<Scene> m_Scene;
    Ref<SceneRenderer> m_Renderer;

    Scene::Entity m_Camera;
    std::vector<DemoLight> m_Lights;
};

void InitializeApplication() {
    Application::PushLayer<TestLayer>();
    Application::PushLayer<ImGuiLayer>();
}

int RunApplication() { return Application::Run(InitializeApplication); }

int main(int argc, const char** argv) {
#ifndef FUUJIN_IS_DEBUG
    try {
        return RunApplication();
    } catch (const std::runtime_error& exc) {
        FUUJIN_CRITICAL("Error thrown: {}", exc.what());
    }
#else
    return RunApplication();
#endif
}
