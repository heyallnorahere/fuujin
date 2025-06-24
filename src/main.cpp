#include "fuujin.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/renderer/ShaderLibrary.h"

using namespace std::chrono_literals;

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
};

static const std::vector<uint32_t> s_Indices = { 0, 1, 2 };
static const std::vector<Vertex> s_Vertices = { { glm::vec3(0.5f, -0.5f, 0.f) },
                                                { glm::vec3(0.f, 0.5f, 0.f) },
                                                { glm::vec3(-0.5f, -0.5f, 0.f) } };

class TestLayer : public fuujin::Layer {
public:
    TestLayer() {
        ZoneScoped;
        LoadResources();

        Hue = 0.f;
    }

    virtual void Update(fuujin::Duration delta) override {
        ZoneScoped;

        static constexpr float hsvMax = 360.f;
        Hue += (float)delta.count() * hsvMax * 0.75f;
        if (Hue > hsvMax) {
            Hue -= hsvMax;
        }

        glm::vec4 color = glm::vec4(hsv2rgb(glm::vec3(Hue, 1.f, 1.f)), 1.f);

        auto pushConstants = fuujin::Buffer::Wrapper(&color, sizeof(glm::vec4));
        fuujin::Renderer::RenderIndexed(m_VertexBuffer, m_IndexBuffer, m_Pipeline,
                                        (uint32_t)s_Indices.size(), pushConstants);
    }

private:
    void LoadResources() {
        ZoneScoped;

        auto context = fuujin::GraphicsContext::Get();
        auto& library = fuujin::Renderer::GetShaderLibrary();

        fuujin::DeviceBuffer::Spec staging, actual;
        staging.Size = actual.Size = s_Vertices.size() * sizeof(Vertex);

        staging.QueueOwnership = { fuujin::QueueType::Transfer };
        staging.Usage = fuujin::DeviceBuffer::Usage::Staging;

        actual.QueueOwnership = { fuujin::QueueType::Graphics, fuujin::QueueType::Transfer };
        actual.Usage = fuujin::DeviceBuffer::Usage::Vertex;

        m_VertexStaging = context->CreateBuffer(staging);
        m_VertexBuffer = context->CreateBuffer(actual);

        staging.Size = actual.Size = s_Indices.size() * sizeof(uint32_t);
        actual.Usage = fuujin::DeviceBuffer::Usage::Index;

        m_IndexStaging = context->CreateBuffer(staging);
        m_IndexBuffer = context->CreateBuffer(actual);

        fuujin::Pipeline::Spec pipelineSpec;
        pipelineSpec.Target = context->GetSwapchain();
        pipelineSpec.Shader = library.Get("assets/shaders/Test.glsl");
        pipelineSpec.Type = fuujin::Pipeline::Type::Graphics;
        pipelineSpec.DisableCulling = true;

        m_Pipeline = context->CreatePipeline(pipelineSpec);

        fuujin::Renderer::Submit([this]() { RT_CopyBuffers(); });
    }

    void RT_CopyBuffers() {
        ZoneScoped;

        auto mapped = m_VertexStaging->RT_Map();
        fuujin::Buffer::Copy(fuujin::Buffer::Wrapper(s_Vertices), mapped);
        m_VertexStaging->RT_Unmap();

        mapped = m_IndexStaging->RT_Map();
        fuujin::Buffer::Copy(fuujin::Buffer::Wrapper(s_Indices), mapped);
        m_IndexStaging->RT_Unmap();

        auto context = fuujin::GraphicsContext::Get();
        auto queue = context->GetQueue(fuujin::QueueType::Transfer);
        auto fence = context->CreateFence();

        auto& cmdList = queue->RT_Get();
        cmdList.RT_Begin();

        m_VertexStaging->RT_CopyTo(cmdList, m_VertexBuffer);
        m_IndexStaging->RT_CopyTo(cmdList, m_IndexBuffer);

        cmdList.AddDependency(m_VertexStaging);
        cmdList.AddDependency(m_IndexStaging);

        m_VertexStaging.Reset();
        m_IndexStaging.Reset();

        cmdList.RT_End();
        queue->RT_Submit(cmdList, fence);
        fence->Wait();
    }

    float Hue;
    fuujin::Ref<fuujin::DeviceBuffer> m_VertexBuffer, m_IndexBuffer;
    fuujin::Ref<fuujin::DeviceBuffer> m_VertexStaging, m_IndexStaging;
    fuujin::Ref<fuujin::Pipeline> m_Pipeline;
};

void InitializeApplication() { fuujin::Application::PushLayer<TestLayer>(); }

int RunApplication() { return fuujin::Application::Run(InitializeApplication); }

int main(int argc, const char** argv) {
    ZoneScoped;

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