#include "fuujin.h"

#include "fuujin/asset/AssetManager.h"
#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/ShaderLibrary.h"
#include "fuujin/renderer/Model.h"

#include <numbers>

using namespace std::chrono_literals;
using namespace fuujin;

// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
static glm::vec3 hsv2rgb(const glm::vec3& in) {
    float hh, p, q, t, ff;
    int32_t i;
    glm::vec3 out;

    if (in.y <= 0.f) { // < is bogus, just shuts up warnings
        out.r = in.z;
        out.g = in.z;
        out.b = in.z;
        return out;
    }
    hh = in.x;
    if (hh >= 360.f)
        hh = 0.f;
    hh /= 60.f;
    i = (long)hh;
    ff = hh - i;
    p = in.z * (1.f - in.y);
    q = in.z * (1.f - (in.y * ff));
    t = in.z * (1.f - (in.y * (1.f - ff)));

    switch (i) {
    case 0:
        out.r = in.z;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.z;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.z;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.z;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.z;
        break;
    case 5:
    default:
        out.r = in.z;
        out.g = p;
        out.b = q;
        break;
    }
    return out;
}

static const std::vector<uint32_t> s_Indices = { 0, 1, 2 };
static const std::vector<fuujin::Vertex> s_Vertices = {
    { glm::vec3(0.5f, -0.5f, 0.f), glm::vec2(1.f, 0.f), glm::vec3(0.f, 0.f, 1.f) },
    { glm::vec3(0.f, 0.5f, 0.f), glm::vec2(0.5f, 1.f), glm::vec3(0.f, 0.f, 1.f) },
    { glm::vec3(-0.5f, -0.5f, 0.f), glm::vec2(0.f, 0.f), glm::vec3(0.f, 0.f, 1.f) }
};

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
        float aspect = (float)width / height;

        if (height == 0) {
            aspect = 1.f;
        }

        float theta = m_Time * s_PI;
        glm::vec3 radial = glm::vec3(glm::sin(theta), 0.f, glm::cos(theta));

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

        glm::vec4 color = glm::vec4(hsv2rgb(glm::vec3(hue, 1.f, 1.f)), 1.f);
        auto material = m_Call.RenderedModel->GetMeshes()[0]->GetMaterial();
        material->SetProperty(Material::Property::AlbedoColor, color);

        Renderer::RenderModel(m_Call);
    }

private:
    void LoadResources() {
        ZoneScoped;

        m_Call.RenderedModel = AssetManager::GetAsset<Model>("fuujin/models/Triangle.model");
        Renderer::Wait();
    }

    float m_Time;

    bool m_ResourcesLoaded;
    ModelRenderCall m_Call;
};

void InitializeApplication() { Application::PushLayer<TestLayer>(); }
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