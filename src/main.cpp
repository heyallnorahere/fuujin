#include "fuujin.h"

#include "fuujin/asset/AssetManager.h"
#include "fuujin/asset/ModelImporter.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/Model.h"

#include "fuujin/animation/Animation.h"

#include "fuujin/imgui/ImGuiLayer.h"

#include <numbers>

#include <imgui.h>

using namespace std::chrono_literals;
using namespace fuujin;

template <typename _Ty>
inline glm::mat<4, 4, _Ty, glm::defaultp> Perspective(_Ty fovy, _Ty aspect, _Ty zNear, _Ty zFar) {
    if (Renderer::GetAPI().LeftHanded) {
        return glm::perspectiveLH(fovy, aspect, zNear, zFar);
    } else {
        return glm::perspectiveRH(fovy, aspect, zNear, zFar);
    }
}

template <typename _Ty, glm::qualifier Q>
inline glm::mat<4, 4, _Ty, Q> LookAt(const glm::vec<3, _Ty, Q>& eye,
                                     const glm::vec<3, _Ty, Q>& center,
                                     const glm::vec<3, _Ty, Q>& up) {
    if (Renderer::GetAPI().LeftHanded) {
        return glm::lookAtLH(eye, center, up);
    } else {
        return glm::lookAtRH(eye, center, up);
    }
}

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

        auto context = Renderer::GetContext();
        auto swapchain = context->GetSwapchain();
        uint32_t width = swapchain->GetWidth();
        uint32_t height = swapchain->GetHeight();

        float aspect;
        if (height > 0) {
            aspect = (float)width / height;
        } else {
            aspect = 1.f;
        }

        float yaw = m_Time * s_PI;
        float sinYaw = glm::sin(yaw);
        float cosYaw = glm::cos(yaw);

        float pitch = sinYaw * s_PI / 8.f;
        float sinPitch = glm::sin(pitch);
        float cosPitch = glm::cos(pitch);

        glm::vec3 radial = glm::vec3(cosYaw * cosPitch, sinPitch, sinYaw * cosPitch);
        glm::vec3 position = radial * 5.f;

        glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
        glm::vec3 direction = -radial;

        Renderer::SceneData scene;
        Renderer::Camera& camera = scene.Cameras.emplace_back();
        camera.Position = position;
        camera.Projection = Perspective(s_PI / 4.f, aspect, 0.1f, 10.f);
        camera.View = LookAt(position, position + direction, up);
        Renderer::UpdateScene(0, scene);

        m_Call.ModelMatrix = glm::mat4(1.f);
        m_Call.SceneID = 0;
        m_Call.CameraIndex = 0;
        m_Call.Target = swapchain;

        static constexpr float hueMax = 360.f;
        float hue = m_Time * hueMax * 0.75f;
        while (hue > hueMax) {
            hue -= hueMax;
        }

        if (m_Animation.IsPresent()) {
            m_AnimationTime += delta;

            auto duration = m_Animation->GetDuration();
            while (m_AnimationTime > duration) {
                m_AnimationTime -= duration;
            }

            const auto& channels = m_Animation->GetChannels();
            for (const auto& channel : channels) {
                auto nodeIndex = m_Call.RenderedModel->FindNode(channel.Name);
                if (!nodeIndex.has_value()) {
                    continue;
                }

                size_t node = nodeIndex.value();

                auto transform = Animation::InterpolateChannel(m_AnimationTime, channel);
                if (transform.has_value()) {
                    m_Call.ModelAnimator->SetLocalTransform(node, transform.value());
                } else {
                    m_Call.ModelAnimator->ResetLocalTransform(node);
                }
            }
        }

        Renderer::RenderModel(m_Call);

        static bool demoOpen = true;
        if (demoOpen) {
            ImGui::ShowDemoWindow(&demoOpen);
        }
    }

private:
    void LoadResources() {
        ZoneScoped;

        static const std::string modelName = "Cube.model";
        static const std::string sourceName = "Cube.gltf";

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

        m_Call.RenderedModel = AssetManager::GetAsset<Model>(modelPath);
        m_Call.ModelAnimator = Ref<Animator>::Create(m_Call.RenderedModel);

        m_Animation = AssetManager::GetAsset<Animation>("fuujin/models/animations/Wave.anim");

        Renderer::Wait();
    }

    float m_Time;
    Duration m_AnimationTime;

    bool m_ResourcesLoaded;
    ModelRenderCall m_Call;

    Ref<Animation> m_Animation;
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
