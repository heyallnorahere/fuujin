#include "fuujin.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/ShaderLibrary.h"

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

struct Vertex {
    glm::vec3 Position;
    glm::vec2 UV;
};

static const std::vector<uint32_t> s_Indices = { 0, 1, 2 };
static const std::vector<Vertex> s_Vertices = {
    { glm::vec3(0.5f, -0.5f, 0.f), glm::vec2(1.f, 1.f) },
    { glm::vec3(0.f, 0.5f, 0.f), glm::vec2(1.f, 0.f) },
    { glm::vec3(-0.5f, -0.5f, 0.f), glm::vec2(0.f, 1.f) }
};

class TestLayer : public Layer {
public:
    TestLayer() {
        ZoneScoped;
        LoadResources();

        Time = 0.f;
    }

    virtual void Update(Duration delta) override {
        ZoneScoped;

        Time += delta.count();

        static const float pi = std::numbers::pi_v<float>;
        float x = std::sin(Time * pi * 2.f) * 0.25f;

        Renderer::Submit([this, x]() {
            auto translation = glm::vec3(x, 0.f, 0.f);

            auto mapped = m_UniformBuffer->RT_Map();
            Buffer::Copy(Buffer::Wrapper(&translation, sizeof(glm::vec3)), mapped);
            m_UniformBuffer->RT_Unmap();
        });

        static constexpr float hueMax = 360.f;
        float hue = Time * hueMax * 0.75f;
        while (hue > hueMax) {
            hue -= hueMax;
        }

        glm::vec4 color = glm::vec4(hsv2rgb(glm::vec3(hue, 1.f, 1.f)), 1.f);
        m_Call.PushConstants = Buffer::Wrapper(&color, sizeof(glm::vec4));

        Renderer::RenderIndexed(m_Call);
    }

private:
    void LoadResources() {
        ZoneScoped;

        auto context = Renderer::GetContext();
        auto& library = Renderer::GetShaderLibrary();

        DeviceBuffer::Spec staging, actual;
        staging.Size = actual.Size = s_Vertices.size() * sizeof(Vertex);

        staging.QueueOwnership = { QueueType::Transfer };
        staging.Usage = DeviceBuffer::Usage::Staging;

        actual.QueueOwnership = { QueueType::Graphics, QueueType::Transfer };
        actual.Usage = DeviceBuffer::Usage::Vertex;

        m_VertexStaging = context->CreateBuffer(staging);
        m_Call.VertexBuffer = context->CreateBuffer(actual);

        staging.Size = actual.Size = s_Indices.size() * sizeof(uint32_t);
        actual.Usage = DeviceBuffer::Usage::Index;

        m_IndexStaging = context->CreateBuffer(staging);
        m_Call.IndexBuffer = context->CreateBuffer(actual);

        Pipeline::Spec pipelineSpec;
        pipelineSpec.Target = context->GetSwapchain();
        pipelineSpec.Shader = library.Get("assets/shaders/Test.glsl");
        pipelineSpec.Type = Pipeline::Type::Graphics;
        pipelineSpec.FrontFace = FrontFace::CCW;

        m_Call.Pipeline = context->CreatePipeline(pipelineSpec);
        m_Call.Resources = Renderer::CreateAllocation(pipelineSpec.Shader);
        m_Call.IndexCount = (uint32_t)s_Indices.size();

        DeviceBuffer::Spec uboSpec;
        uboSpec.QueueOwnership = { QueueType::Graphics };
        uboSpec.Size = sizeof(glm::vec3);
        uboSpec.Usage = DeviceBuffer::Usage::Uniform;

        m_UniformBuffer = context->CreateBuffer(uboSpec);
        m_Texture = Renderer::LoadTexture("assets/textures/texture.png");

        m_Call.Resources->Bind("TransformUBO", m_UniformBuffer);
        m_Call.Resources->Bind("u_Texture", m_Texture);

        fuujin::Renderer::Submit([this]() { RT_CopyBuffers(); });
    }

    void RT_CopyBuffers() {
        ZoneScoped;

        auto mapped = m_VertexStaging->RT_Map();
        Buffer::Copy(Buffer::Wrapper(s_Vertices), mapped);
        m_VertexStaging->RT_Unmap();

        mapped = m_IndexStaging->RT_Map();
        Buffer::Copy(Buffer::Wrapper(s_Indices), mapped);
        m_IndexStaging->RT_Unmap();

        auto context = Renderer::GetContext();
        auto queue = context->GetQueue(fuujin::QueueType::Transfer);
        auto fence = context->CreateFence();

        auto& cmdList = queue->RT_Get();
        cmdList.RT_Begin();

        m_VertexStaging->RT_CopyToBuffer(cmdList, m_Call.VertexBuffer);
        m_IndexStaging->RT_CopyToBuffer(cmdList, m_Call.IndexBuffer);

        cmdList.AddDependency(m_VertexStaging);
        cmdList.AddDependency(m_IndexStaging);

        m_VertexStaging.Reset();
        m_IndexStaging.Reset();

        cmdList.RT_End();
        queue->RT_Submit(cmdList, fence);
        fence->Wait();
    }

    float Time;

    IndexedRenderCall m_Call;
    Ref<DeviceBuffer> m_UniformBuffer;
    Ref<Texture> m_Texture;
    Ref<DeviceBuffer> m_VertexStaging, m_IndexStaging;
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