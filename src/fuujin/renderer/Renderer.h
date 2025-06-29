#pragma once

#include "fuujin/core/Ref.h"
#include "fuujin/core/Buffer.h"
#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/renderer/Framebuffer.h"
#include "fuujin/renderer/DeviceBuffer.h"
#include "fuujin/renderer/Pipeline.h"
#include "fuujin/renderer/Texture.h"
#include "fuujin/renderer/Material.h"

namespace fuujin {
    class Event;
    class ShaderLibrary;
    class CommandList;

    class RendererAllocation : public RefCounted {
    public:
        virtual bool Bind(const std::string& name, Ref<DeviceBuffer> buffer, uint32_t index = 0,
                          size_t offset = 0, size_t range = 0) = 0;

        virtual bool Bind(const std::string& name, Ref<Texture> texture, uint32_t index = 0) = 0;
    };

    struct IndexedRenderCall {
        Ref<DeviceBuffer> VertexBuffer, IndexBuffer;
        Ref<Pipeline> Pipeline;
        uint32_t IndexCount;

        Buffer PushConstants;
        std::vector<Ref<RendererAllocation>> Resources;
    };

    struct MaterialRenderCall {
        Ref<DeviceBuffer> VertexBuffer, IndexBuffer;
        Ref<Pipeline> Pipeline;
        uint32_t IndexCount;
        uint64_t SceneID;

        glm::mat4 ModelMatrix;
        size_t CameraIndex;

        Ref<Material> Material;
    };

    class RendererAPI {
    public:
        virtual ~RendererAPI() = default;

        virtual void RT_NewFrame(uint32_t frame) = 0;
        virtual void RT_PrePresent(CommandList& cmdlist) = 0;

        virtual void RT_RenderIndexed(CommandList& cmdlist, const IndexedRenderCall& data) = 0;

        virtual void RT_SetViewport(CommandList& cmdlist, Ref<RenderTarget> target) const = 0;

        virtual Ref<RendererAllocation> CreateAllocation(const Ref<Shader>& shader) const = 0;
    };

    class Renderer {
    public:
        struct Camera {
            glm::vec3 Position;
            glm::mat4 Projection, View;
        };

        struct SceneData {
            std::vector<Camera> Cameras;
        };

        Renderer() = delete;

        static void Init();
        static void Shutdown();

        static Ref<GraphicsContext> GetContext();
        static Ref<CommandQueue> GetGraphicsQueue();
        static ShaderLibrary& GetShaderLibrary();
        static const GraphicsDevice::API& GetAPI();

        static void ProcessEvent(Event& event);

        static Ref<RendererAllocation> CreateAllocation(const Ref<Shader>& shader);

        static void FreeShader(uint64_t id);
        static void FreeMaterial(uint64_t id);
        static void FreeScene(uint64_t id);

        static void UpdateScene(uint64_t id, const SceneData& data);
        static Ref<RendererAllocation> GetSceneAllocation(uint64_t id, const Ref<Shader>& shader);

        static Ref<RendererAllocation> GetMaterialAllocation(const Ref<Material>& material,
                                                             const Ref<Shader>& shader);

        static Ref<Texture> LoadTexture(const fs::path& path, const Ref<Sampler>& sampler = {});
        static Ref<Texture> CreateTexture(uint32_t width, uint32_t height, Texture::Format format,
                                          const Buffer& data, const Ref<Sampler>& sampler = {},
                                          const fs::path& path = fs::path());

        // submits a job to the render thread
        // all jobs will be executed in order submitted
        // if a job is submitted inside the render thread, the job will be executed immediately
        static void Submit(const std::function<void()>& callback,
                           const std::optional<std::string>& label = {});

        // waits for all jobs on the render thread to finish
        // returns if the jobs actually finished
        static bool Wait(std::optional<std::chrono::milliseconds> timeout = {});

        // checks if the thread this is called from is the render thread
        static bool IsRenderThread();

        // signals a new frame
        static void NewFrame();

        // pushes a render target of a framebuffer
        static void PushRenderTarget(Ref<RenderTarget> target);

        // flushes and pops a render target from the stack
        static void PopRenderTarget();

        // renders a mesh with the provided resources
        // note that a render target needs to have been pushed
        static void RenderIndexed(const IndexedRenderCall& data);

        static void RenderWithMaterial(const MaterialRenderCall& data);

    private:
        static void CreateDefaultObjects();
    };
} // namespace fuujin