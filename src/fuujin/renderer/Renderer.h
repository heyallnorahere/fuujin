#pragma once

#include "fuujin/core/Ref.h"
#include "fuujin/core/Buffer.h"

#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/renderer/Framebuffer.h"
#include "fuujin/renderer/DeviceBuffer.h"
#include "fuujin/renderer/Pipeline.h"
#include "fuujin/renderer/Texture.h"
#include "fuujin/renderer/Material.h"
#include "fuujin/renderer/Model.h"
#include "fuujin/renderer/Light.h"

#include "fuujin/animation/Animator.h"

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

    struct Scissor {
        int32_t X, Y;
        uint32_t Width, Height;
    };

    struct IndexedRenderCall {
        // these two should not be handled by the renderer backend
        std::optional<bool> Flip;
        std::optional<Scissor> ScissorRect;

        std::vector<Ref<DeviceBuffer>> VertexBuffers;
        Ref<DeviceBuffer> IndexBuffer;
        Ref<Pipeline> RenderPipeline;

        int32_t VertexOffset = 0;
        uint32_t IndexOffset = 0;
        uint32_t IndexCount = 0;

        Buffer PushConstants;
        std::vector<Ref<RendererAllocation>> Resources;
    };

    struct MaterialRenderCall {
        std::vector<Ref<DeviceBuffer>> VertexBuffers;
        Ref<DeviceBuffer> IndexBuffer;
        Ref<Pipeline> RenderPipeline;
        uint32_t IndexCount;

        glm::mat4 ModelMatrix;
        size_t FirstCamera, CameraCount;
        std::unordered_map<std::string, Buffer> PushConstants;

        uint64_t SceneID;
        Ref<Material> RenderMaterial;
        std::vector<Ref<RendererAllocation>> AdditionalResources;
    };

    struct ModelRenderCall {
        Ref<Model> RenderedModel;
        Ref<Animator> ModelAnimator;

        uint64_t SceneID;

        glm::mat4 ModelMatrix;
        size_t FirstCamera, CameraCount;
    };

    class RendererAPI {
    public:
        virtual ~RendererAPI() = default;

        virtual void RT_NewFrame(uint32_t frame) = 0;
        virtual void RT_PrePresent(CommandList& cmdlist) = 0;

        virtual void RT_RenderIndexed(CommandList& cmdlist, const IndexedRenderCall& data) = 0;

        virtual void RT_SetViewport(CommandList& cmdlist, Ref<RenderTarget> target,
                                    const std::optional<bool>& flip,
                                    const std::optional<Scissor>& scissor) const = 0;

        virtual Ref<RendererAllocation> CreateAllocation(const Ref<Shader>& shader) const = 0;
    };

    class Renderer {
    public:
        struct Camera {
            glm::vec3 Position;
            glm::mat4 ViewProjection;
        };

        struct RendererLight {
            glm::mat4 TransformMatrix;
            Ref<Light> LightData;
        };

        struct SceneData {
            std::vector<Camera> Cameras;
            std::vector<RendererLight> Lights;
        };

        struct MeshBuffers {
            std::vector<Ref<DeviceBuffer>> VertexBuffers;
            Ref<DeviceBuffer> IndexBuffer;
        };

        Renderer() = delete;

        static void Init();
        static void Shutdown();

        static Ref<GraphicsContext> GetContext();
        static Ref<CommandQueue> GetGraphicsQueue();
        static ShaderLibrary& GetShaderLibrary();
        static const GraphicsDevice::APISpec& GetAPI();

        static void ProcessEvent(Event& event);

        static Ref<RendererAllocation> CreateAllocation(const Ref<Shader>& shader);

        static void FreeShader(uint64_t id);
        static void FreeMaterial(uint64_t id);
        static void FreeScene(uint64_t id);
        static void FreeMesh(uint64_t id);
        static void FreeAnimator(uint64_t id);

        static void UpdateScene(uint64_t id, const SceneData& data);
        static Ref<RendererAllocation> GetSceneAllocation(uint64_t id, const Ref<Shader>& shader);

        static Ref<RendererAllocation> GetMaterialAllocation(const Ref<Material>& material,
                                                             const Ref<Shader>& shader);

        static Ref<Pipeline> GetMaterialPipeline(const Ref<Shader>& shader,
                                                 const Ref<RenderTarget>& target,
                                                 const Material::PipelineProperties& spec);

        static const MeshBuffers& GetMeshBuffers(const std::unique_ptr<Mesh>& mesh);

        static Ref<RendererAllocation> GetAnimatorAllocation(const Ref<Animator>& animator,
                                                             const Ref<Shader>& shader);

        static Ref<Texture> CreateTexture(uint32_t width, uint32_t height, Texture::Format format,
                                          const Buffer& data, const Ref<Sampler>& sampler = {},
                                          const fs::path& path = fs::path());

        static void UpdateTexture(const Ref<Texture>& texture, const Buffer& data,
                                  const Scissor& scissor);

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

        static Ref<RenderTarget> GetActiveRenderTarget();

        // renders a mesh with the provided resources
        // note that a render target needs to have been pushed
        static void RenderIndexed(const IndexedRenderCall& data);

        static void RenderWithMaterial(const MaterialRenderCall& data);

        static void RenderModel(const ModelRenderCall& data);

    private:
        static void CreateDefaultObjects();

        static size_t HashMaterialPipelineSpec(const Material::PipelineProperties& spec,
                                               uint32_t renderTarget);
    };
} // namespace fuujin
