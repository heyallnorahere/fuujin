#include "fuujinpch.h"
#include "fuujin/renderer/Renderer.h"

#include "fuujin/renderer/ShaderLibrary.h"
#include "fuujin/renderer/ShaderBuffer.h"
#include "fuujin/renderer/Model.h"
#include "fuujin/renderer/Framebuffer.h"

#include "fuujin/core/Events.h"

#include <thread>
#include <queue>
#include <mutex>
#include <stack>

#include <spdlog/stopwatch.h>
#include <spdlog/fmt/chrono.h>

using namespace std::chrono_literals;

namespace fuujin {
    struct QueueCallback {
        std::function<void()> Callback;
        std::string Label;
    };

    struct ActiveRenderTarget {
        Ref<RenderTarget> Target;
        CommandList* CmdList;
        bool ResetViewport;
    };

    struct ObjectAllocation {
        Ref<RendererAllocation> Allocation;
        Ref<DeviceBuffer> Buffer;
        uint64_t State;
        bool IsNew;
    };

    struct RendererShaderData {
        std::unordered_map<uint64_t, ObjectAllocation> Materials, Scenes, Animators;
        std::unordered_map<size_t, Ref<Pipeline>> MaterialPipelines;
    };

    struct RendererSceneState {
        uint64_t State;
        Renderer::SceneData Data;
    };

    struct RendererData {
        struct {
            std::thread Thread;
            std::thread::id ID;
            std::mutex Mutex;
            std::queue<QueueCallback> Queue;
        } RenderThread;

        GraphicsDevice::Properties DeviceProperties;
        Ref<GraphicsContext> Context;
        Ref<CommandQueue> GraphicsQueue;
        std::unique_ptr<ShaderLibrary> Library;
        RendererAPI* API;

        Ref<Sampler> DefaultSampler;
        Ref<Texture> WhiteTexture;

        std::unordered_map<uint64_t, RendererSceneState> SceneState;
        std::unordered_map<uint64_t, RendererShaderData> ShaderData;
        std::unordered_map<uint64_t, Renderer::MeshBuffers> MeshBuffers;

        uint32_t FrameCount;
        std::optional<uint32_t> CurrentFrame;

        // use shared_ptr to keep structure in same place in memory
        std::stack<std::shared_ptr<ActiveRenderTarget>> Targets;
    };

    static std::unique_ptr<RendererData> s_Data;
    static void RenderThread() {
        tracy::SetThreadName("Render thread");
        s_Data->RenderThread.ID = std::this_thread::get_id();

        FUUJIN_DEBUG("Render thread starting...");

        while (s_Data) {
            bool doSleep = false;

            {
                auto& queue = s_Data->RenderThread.Queue;
                std::optional<QueueCallback> callback;

                {
                    std::lock_guard lock(s_Data->RenderThread.Mutex);
                    if (!queue.empty()) {
                        callback = queue.front();
                    }
                }

                if (callback.has_value()) {
                    FUUJIN_TRACE("Render thread: {}", callback->Label.c_str());
                    callback->Callback();

                    {
                        std::lock_guard lock(s_Data->RenderThread.Mutex);
                        queue.pop();
                    }
                } else {
                    doSleep = true;
                }
            }

            if (doSleep) {
                std::this_thread::sleep_for(1ms);
            }
        }
    }

    static void LogGraphicsContext() {
        ZoneScoped;
        auto device = s_Data->Context->GetDevice();

        const auto& properties = s_Data->DeviceProperties;
        const auto& api = properties.API;

        std::string type;
        switch (properties.Type) {
        case GraphicsDeviceType::Discrete:
            type = "Discrete";
            break;
        case GraphicsDeviceType::CPU:
            type = "CPU";
            break;
        case GraphicsDeviceType::Integrated:
            type = "Integrated";
            break;
        case GraphicsDeviceType::Other:
        default:
            type = "Other";
            break;
        }

        FUUJIN_INFO("Graphics device:");
        FUUJIN_INFO("\tName: {}", properties.Name.c_str());
        FUUJIN_INFO("\tVendor: {}", properties.Vendor.c_str());
        FUUJIN_INFO("\tType: {}", type.c_str());

        FUUJIN_INFO("\tDriver version: {}.{}.{}", properties.DriverVersion.Major,
                    properties.DriverVersion.Minor, properties.DriverVersion.Patch);

        FUUJIN_INFO("API: {}", api.Name.c_str());
        FUUJIN_INFO("\tVersion: {}.{}.{}", api.APIVersion.Major, api.APIVersion.Minor,
                    api.APIVersion.Patch);

        FUUJIN_INFO("\tTranspose matrices? {}", api.TransposeMatrices ? "yes" : "no");
        FUUJIN_INFO("\tLeft handed? {}", api.LeftHanded ? "yes" : "no");
    }

    void Renderer::CreateDefaultObjects() {
        ZoneScoped;

        Sampler::Spec spec{}; // default (linear, repeat)
        s_Data->DefaultSampler = s_Data->Context->CreateSampler(spec);

        Buffer whiteData(sizeof(uint8_t) * 4);
        std::memset(whiteData.Get(), 0xFF, whiteData.GetSize());
        s_Data->WhiteTexture = CreateTexture(1, 1, Texture::Format::RGBA8, whiteData);
    }

    void Renderer::Init() {
        ZoneScoped;

        if (s_Data) {
            FUUJIN_WARN("Renderer already initialized - skipping second initialization");
            return;
        }

        s_Data = std::make_unique<RendererData>();
        s_Data->RenderThread.Thread = std::thread(RenderThread);
        s_Data->RenderThread.Thread.detach();

        s_Data->Context = GraphicsContext::Get();
        s_Data->Library = std::make_unique<ShaderLibrary>(s_Data->Context);

        s_Data->Context->GetDevice()->GetProperties(s_Data->DeviceProperties);
        Renderer::Submit([]() { LogGraphicsContext(); });

        uint32_t frameCount = 3;

        s_Data->API = s_Data->Context->CreateRendererAPI(frameCount);
        s_Data->FrameCount = frameCount;

        Renderer::Submit(
            []() { s_Data->GraphicsQueue = s_Data->Context->GetQueue(QueueType::Graphics); },
            "Fetch graphics queue");

        CreateDefaultObjects();
    }

    void Renderer::Shutdown() {
        ZoneScoped;
        if (!s_Data) {
            FUUJIN_WARN("Renderer not initialized - skipping");
            return;
        }

        Wait();
        delete s_Data->API;

        s_Data->MeshBuffers.clear();
        s_Data->ShaderData.clear();
        s_Data->WhiteTexture.Reset();
        s_Data->DefaultSampler.Reset();

        s_Data->Library.reset();
        s_Data->GraphicsQueue.Reset();
        s_Data->Context.Reset();

        Wait();
        s_Data.reset();
    }

    Ref<GraphicsContext> Renderer::GetContext() {
        ZoneScoped;
        if (!s_Data) {
            return nullptr;
        }

        return s_Data->Context;
    }

    Ref<CommandQueue> Renderer::GetGraphicsQueue() {
        ZoneScoped;
        if (!s_Data) {
            return nullptr;
        }

        return s_Data->GraphicsQueue;
    }

    ShaderLibrary& Renderer::GetShaderLibrary() {
        ZoneScoped;
        if (!s_Data) {
            throw std::runtime_error("Renderer has not been initialized!");
        }

        return *s_Data->Library;
    }

    const GraphicsDevice::APISpec& Renderer::GetAPI() {
        ZoneScoped;
        if (!s_Data) {
            throw std::runtime_error("Renderer has not been initialized!");
        }

        return s_Data->DeviceProperties.API;
    }

    void Renderer::ProcessEvent(Event& event) {
        ZoneScoped;

        if (event.GetType() == EventType::ViewResized) {
            auto swapchain = s_Data->Context->GetSwapchain();
            if (swapchain.IsPresent()) {
                auto& resizeEvent = (ViewResizedEvent&)event;
                auto view = swapchain->GetView();

                if (resizeEvent.GetView() == view->GetID()) {
                    swapchain->RequestResize(resizeEvent.GetFramebufferSize());
                }
            }
        }
    }

    Ref<RendererAllocation> Renderer::CreateAllocation(const Ref<Shader>& shader) {
        ZoneScoped;
        return s_Data->API->CreateAllocation(shader);
    }

    void Renderer::FreeShader(uint64_t id) {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        s_Data->ShaderData.erase(id);
    }

    void Renderer::FreeMaterial(uint64_t id) {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        for (auto& [shaderID, data] : s_Data->ShaderData) {
            data.Materials.erase(id);
        }
    }

    void Renderer::FreeScene(uint64_t id) {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        s_Data->SceneState.erase(id);
        for (auto& [shaderID, data] : s_Data->ShaderData) {
            data.Scenes.erase(id);
        }
    }

    void Renderer::FreeMesh(uint64_t id) {
        ZoneScoped;
        if (!s_Data || !s_Data->MeshBuffers.contains(id)) {
            return;
        }

        s_Data->MeshBuffers.erase(id);
    }

    void Renderer::FreeAnimator(uint64_t id) {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        for (auto& [shaderID, shaderData] : s_Data->ShaderData) {
            shaderData.Animators.erase(id);
        }
    }

    void Renderer::UpdateScene(uint64_t id, const SceneData& data) {
        ZoneScoped;
        if (!s_Data) {
            return;
        }

        if (!s_Data->SceneState.contains(id)) {
            s_Data->SceneState[id].State = 0;
        }

        auto& state = s_Data->SceneState[id];
        state.Data = data;
        state.State++;
    }

    static void CreateObjectAllocation(const Ref<Shader>& shader, const std::string& bufferName,
                                       ObjectAllocation& allocation) {
        ZoneScoped;

        allocation.Allocation = Renderer::CreateAllocation(shader);
        allocation.State = 0;
        allocation.IsNew = true;

        auto resource = shader->GetResourceByName(bufferName);
        if (resource) {
            DeviceBuffer::Spec bufferSpec;
            bufferSpec.QueueOwnership = { QueueType::Graphics };
            bufferSpec.Size = resource->GetType()->GetSize();
            bufferSpec.BufferUsage = DeviceBuffer::Usage::Uniform;

            allocation.Buffer = s_Data->Context->CreateBuffer(bufferSpec);
            allocation.Allocation->Bind(bufferName, allocation.Buffer);
        }
    }

    struct AllocationUniformSet {
        ShaderBuffer Data;
        Ref<DeviceBuffer> UniformBuffer;
    };

    template <typename _Ty>
    static bool UpdateObjectAllocation(const Ref<Shader>& shader, const std::string& bufferName,
                                       ObjectAllocation& allocation, uint64_t currentState,
                                       const _Ty& bufferCallback) {
        ZoneScoped;

        if (currentState != allocation.State || allocation.IsNew) {
            allocation.State = currentState;
            allocation.IsNew = false;

            auto bufferResource = shader->GetResourceByName(bufferName);
            if (bufferResource) {
                auto uniformSet = new AllocationUniformSet;
                uniformSet->UniformBuffer = allocation.Buffer;

                uniformSet->Data = ShaderBuffer(bufferResource->GetType());
                auto& dataBuffer = uniformSet->Data.GetBuffer();
                std::memset(dataBuffer.Get(), 0, dataBuffer.GetSize());
                bufferCallback(uniformSet->Data);

                Renderer::Submit([uniformSet]() {
                    auto mapped = uniformSet->UniformBuffer->RT_Map();
                    Buffer::Copy(uniformSet->Data.GetBuffer(), mapped);
                    uniformSet->UniformBuffer->RT_Unmap();

                    delete uniformSet;
                });
            }

            return true;
        }

        return false;
    }

    Ref<RendererAllocation> Renderer::GetSceneAllocation(uint64_t id, const Ref<Shader>& shader) {
        ZoneScoped;
        if (!s_Data || !s_Data->SceneState.contains(id)) {
            return nullptr;
        }

        // see assets/shaders/include/Scene.glsl
        static const std::string sceneBufferName = "Scene";

        uint64_t shaderID = shader->GetID();
        auto& shaderData = s_Data->ShaderData[shaderID];

        if (!shaderData.Scenes.contains(id)) {
            auto& newAllocation = shaderData.Scenes[id];
            CreateObjectAllocation(shader, sceneBufferName, newAllocation);
        }

        const auto& scene = s_Data->SceneState.at(id);
        auto callback = [&](ShaderBuffer& buffer) {
            const auto& data = scene.Data;

            // see assets/shaders/include/Scene.glsl
            for (size_t i = 0; i < data.Cameras.size(); i++) {
                const auto& camera = data.Cameras[i];
                std::string basePath = "Cameras[" + std::to_string(i) + "]";

                ShaderBuffer cameraSlice;
                if (!buffer.Slice(basePath, cameraSlice)) {
                    continue;
                }

                cameraSlice.Set("Position", camera.Position);
                cameraSlice.Set("ViewProjection", camera.ViewProjection);
            }

            size_t lightCount = data.Lights.size();
            buffer.Set("LightCount", (int32_t)lightCount);

            for (size_t i = 0; i < lightCount; i++) {
                const auto& light = data.Lights[i];
                std::string basePath = "Lights[" + std::to_string(i) + "]";

                ShaderBuffer lightSlice;
                if (!buffer.Slice(basePath, lightSlice)) {
                    continue;
                }

                light.LightData->SetUniforms(lightSlice, light.TransformMatrix);
            }
        };

        auto& allocation = shaderData.Scenes[id];
        UpdateObjectAllocation(shader, sceneBufferName, allocation, scene.State, callback);

        return allocation.Allocation;
    }

    Ref<RendererAllocation> Renderer::GetMaterialAllocation(const Ref<Material>& material,
                                                            const Ref<Shader>& shader) {
        ZoneScoped;

        // see assets/shaders/include/Material.glsl
        static const std::string materialBufferName = "Material";

        uint64_t shaderID = shader->GetID();
        auto& shaderData = s_Data->ShaderData[shaderID];

        uint64_t materialID = material->GetID();
        if (!shaderData.Materials.contains(materialID)) {
            auto& newAllocation = shaderData.Materials[materialID];
            CreateObjectAllocation(shader, materialBufferName, newAllocation);
        }

        auto& allocation = shaderData.Materials[materialID];
        uint64_t currentState = material->GetState();

        auto callback = [&](ShaderBuffer& buffer) { material->MapProperties(buffer); };
        if (UpdateObjectAllocation(shader, materialBufferName, allocation, currentState,
                                   callback)) {
            const auto& textures = material->GetTextures();
            for (uint32_t i = 0; i < (uint32_t)Material::TextureSlot::MAX; i++) {
                auto slot = (Material::TextureSlot)i;
                auto name = Material::GetTextureName(slot);

                auto texture = textures.contains(slot) ? textures.at(slot) : s_Data->WhiteTexture;
                allocation.Allocation->Bind(name, texture);
            }
        }

        return allocation.Allocation;
    }

    size_t Renderer::HashMaterialPipelineSpec(const Material::PipelineProperties& spec,
                                              uint32_t renderTarget) {
        ZoneScoped;
        size_t hash = 0;

        hash |= renderTarget;

        if (spec.Wireframe) {
            hash |= (1 << 8);
        }

        return hash;
    }

    Ref<Pipeline> Renderer::GetMaterialPipeline(const Ref<Shader>& shader,
                                                const Ref<RenderTarget>& target,
                                                const Material::PipelineProperties& spec) {
        ZoneScoped;
        if (!s_Data) {
            return nullptr;
        }

        uint64_t shaderID = shader->GetID();
        auto& shaderData = s_Data->ShaderData[shaderID];

        size_t hash = HashMaterialPipelineSpec(spec, target->GetID());
        if (shaderData.MaterialPipelines.contains(hash)) {
            return shaderData.MaterialPipelines.at(hash);
        }

        Pipeline::Spec pipelineSpec;
        pipelineSpec.PipelineType = Pipeline::Type::Graphics;
        pipelineSpec.PipelineShader = shader;
        pipelineSpec.Target = target;
        pipelineSpec.PolygonFrontFace = FrontFace::CCW;
        pipelineSpec.Wireframe = spec.Wireframe;

        // skinning attributes (bone IDs, weights)
        pipelineSpec.AttributeBindings[4] = 1;
        pipelineSpec.AttributeBindings[5] = 1;

        return shaderData.MaterialPipelines[hash] = s_Data->Context->CreatePipeline(pipelineSpec);
    }

    template <glm::length_t L>
    struct SizedBoneVertex {
        static constexpr glm::length_t MaxBones = L;

        glm::vec<L, int32_t, glm::defaultp> Indices;
        glm::vec<L, float, glm::defaultp> Weights;
    };

    using BoneVertex = SizedBoneVertex<4>;

    struct MeshLoadData {
        Renderer::MeshBuffers Actual, Staging;
        std::vector<Buffer> Vertices;
        std::vector<uint32_t> Indices;
    };

    static void RT_PopulateMeshBuffers(MeshLoadData* data) {
        ZoneScoped;

        auto queue = s_Data->Context->GetQueue(QueueType::Transfer);
        auto& cmdlist = queue->RT_Get();
        cmdlist.RT_Begin();

        auto mapped = data->Staging.IndexBuffer->RT_Map();
        Buffer::Copy(Buffer::Wrapper(data->Indices), mapped);
        data->Staging.IndexBuffer->RT_Unmap();

        data->Staging.IndexBuffer->RT_CopyToBuffer(cmdlist, data->Actual.IndexBuffer);
        cmdlist.AddDependency(data->Staging.IndexBuffer);
        cmdlist.AddDependency(data->Actual.IndexBuffer);

        for (size_t i = 0; i < data->Vertices.size(); i++) {
            const auto& vertexData = data->Vertices[i];
            const auto& staging = data->Staging.VertexBuffers[i];
            const auto& actual = data->Actual.VertexBuffers[i];

            mapped = staging->RT_Map();
            Buffer::Copy(vertexData, mapped);
            staging->RT_Unmap();

            staging->RT_CopyToBuffer(cmdlist, actual);
            cmdlist.AddDependency(staging);
            cmdlist.AddDependency(actual);
        }

        cmdlist.RT_End();
        queue->RT_Submit(cmdlist);

        delete data;
    }

    const Renderer::MeshBuffers& Renderer::GetMeshBuffers(const std::unique_ptr<Mesh>& mesh) {
        ZoneScoped;

        uint64_t id = mesh->GetID();
        if (!s_Data->MeshBuffers.contains(id)) {
            FUUJIN_INFO("Creating vertex and index buffers for mesh {}", id);

            const auto& vertices = mesh->GetVertices();

            auto data = new MeshLoadData;
            data->Vertices.push_back(Buffer::Wrapper(vertices).Copy());
            data->Indices = mesh->GetIndices();

            const auto& bones = mesh->GetBones();
            if (!bones.empty()) {
                FUUJIN_INFO("Generating bone vertices for mesh {}", id);

                size_t vertexCount = vertices.size();
                std::vector<size_t> boneCount(vertexCount, 0);
                std::vector<BoneVertex> boneVertices(vertexCount, BoneVertex{});

                for (const auto& bone : bones) {
                    for (const auto& [index, weight] : bone.Weights) {
                        auto& vertexBoneCount = boneCount[index];
                        auto& vertex = boneVertices[index];

                        if (vertexBoneCount >= BoneVertex::MaxBones) {
                            throw std::runtime_error("Cannot have more than " +
                                                     std::to_string(BoneVertex::MaxBones) +
                                                     " bones per vertex!");
                        }

                        vertex.Indices[vertexBoneCount] = (int32_t)bone.Index;
                        vertex.Weights[vertexBoneCount] = weight;
                        vertexBoneCount++;
                    }
                }

                data->Vertices.push_back(Buffer::Wrapper(boneVertices).Copy());
            }

            DeviceBuffer::Spec staging, actual;
            staging.QueueOwnership = { QueueType::Transfer };
            staging.BufferUsage = DeviceBuffer::Usage::Staging;
            actual.QueueOwnership = { QueueType::Transfer, QueueType::Graphics };
            actual.BufferUsage = DeviceBuffer::Usage::Index;
            actual.Size = staging.Size = data->Indices.size() * sizeof(uint32_t);

            data->Staging.IndexBuffer = s_Data->Context->CreateBuffer(staging);
            data->Actual.IndexBuffer = s_Data->Context->CreateBuffer(actual);

            actual.BufferUsage = DeviceBuffer::Usage::Vertex;
            for (const auto& vertices : data->Vertices) {
                actual.Size = staging.Size = vertices.GetSize();

                data->Actual.VertexBuffers.push_back(s_Data->Context->CreateBuffer(actual));
                data->Staging.VertexBuffers.push_back(s_Data->Context->CreateBuffer(staging));
            }

            s_Data->MeshBuffers[id] = data->Actual;
            Renderer::Submit([data] { RT_PopulateMeshBuffers(data); });
        }

        return s_Data->MeshBuffers[id];
    }

    Ref<RendererAllocation> Renderer::GetAnimatorAllocation(const Ref<Animator>& animator,
                                                            const Ref<Shader>& shader) {
        ZoneScoped;

        const auto& boneTransforms = animator->GetBoneTransforms();
        if (boneTransforms.empty()) {
            return nullptr; // no point
        }

        // see assets/shaders/MaterialSkinned.glsl
        static const std::string boneBufferName = "Bones";

        if (!shader->GetResourceByName(boneBufferName)) {
            return nullptr;
        }

        uint64_t shaderID = shader->GetID();
        auto& shaderData = s_Data->ShaderData[shaderID];

        uint64_t animatorID = animator->GetID();
        if (!shaderData.Animators.contains(animatorID)) {
            auto& newAllocation = shaderData.Animators[animatorID];
            CreateObjectAllocation(shader, boneBufferName, newAllocation);
        }

        auto callback = [&](ShaderBuffer& buffer) {
            for (size_t i = 0; i < boneTransforms.size(); i++) {
                buffer.Set("Transforms[" + std::to_string(i) + "]", boneTransforms[i]);
            }
        };

        auto& allocation = shaderData.Animators[animatorID];
        UpdateObjectAllocation(shader, boneBufferName, allocation, animator->GetState(), callback);

        return allocation.Allocation;
    }

    struct TextureLoadInfo {
        Ref<DeviceBuffer> StagingBuffer;
        Ref<Texture> Instance;
        Buffer ImageData;
        Scissor CopyRect;
    };

    static void RT_LoadTextureData(TextureLoadInfo* loadInfo) {
        ZoneScoped;

        auto mapped = loadInfo->StagingBuffer->RT_Map();
        Buffer::Copy(loadInfo->ImageData, mapped);
        loadInfo->StagingBuffer->RT_Unmap();

        auto transferQueue = s_Data->Context->GetQueue(QueueType::Transfer);
        auto& cmdlist = transferQueue->RT_Get();
        cmdlist.RT_Begin();

        ImageCopy copy{};
        copy.X = loadInfo->CopyRect.X;
        copy.Y = loadInfo->CopyRect.Y;
        copy.Width = loadInfo->CopyRect.Width;
        copy.Height = loadInfo->CopyRect.Height;
        copy.Depth = 1;
        copy.ArraySize = 1;

        auto image = loadInfo->Instance->GetImage();
        loadInfo->StagingBuffer->RT_CopyToImage(cmdlist, image, 0, copy);
        cmdlist.AddDependency(loadInfo->StagingBuffer);

        if (loadInfo->Instance->GetSpec().MipLevels > 1) {
            // todo: generate mipmaps
        }

        cmdlist.RT_End();
        transferQueue->RT_Submit(cmdlist);

        delete loadInfo;
    }

    Ref<Texture> Renderer::CreateTexture(uint32_t width, uint32_t height, Texture::Format format,
                                         const Buffer& data, const Ref<Sampler>& sampler,
                                         const fs::path& path) {
        ZoneScoped;
        if (!s_Data) {
            FUUJIN_ERROR(
                "Attempted to create texture with renderer not initialized! Returning null");

            return nullptr;
        }

        if (data.IsEmpty()) {
            FUUJIN_ERROR("Attempted to create texture with an empty buffer! Returning null");
            return nullptr;
        }

        Texture::Spec textureSpec;
        textureSpec.TextureSampler = sampler.IsPresent() ? sampler : s_Data->DefaultSampler;
        textureSpec.Path = path;

        textureSpec.Width = width;
        textureSpec.Height = height;
        textureSpec.Depth = 1;
        textureSpec.MipLevels = 1; // todo: mipmaps
        textureSpec.ImageFormat = format;
        textureSpec.TextureType = Texture::Type::_2D;

        Scissor scissor;
        scissor.X = 0;
        scissor.Y = 0;
        scissor.Width = width;
        scissor.Height = height;

        auto texture = s_Data->Context->CreateTexture(textureSpec);
        UpdateTexture(texture, data, scissor);

        return texture;
    }

    void Renderer::UpdateTexture(const Ref<Texture>& texture, const Buffer& data,
                                 const Scissor& scissor) {
        ZoneScoped;

        DeviceBuffer::Spec stagingSpec;
        stagingSpec.QueueOwnership = { QueueType::Transfer };
        stagingSpec.Size = data.GetSize();
        stagingSpec.BufferUsage = DeviceBuffer::Usage::Staging;

        auto loadInfo = new TextureLoadInfo;
        loadInfo->StagingBuffer = s_Data->Context->CreateBuffer(stagingSpec);
        loadInfo->Instance = texture;
        loadInfo->ImageData = data;
        loadInfo->CopyRect = scissor;

        Renderer::Submit([=]() { RT_LoadTextureData(loadInfo); });
    }

    void Renderer::Submit(const std::function<void()>& callback,
                          const std::optional<std::string>& label) {
        auto jobLabel = label.value_or("<unnamed render task>");
        if (IsRenderThread()) {
            FUUJIN_TRACE("Render thread: inline sub-job {}", jobLabel.c_str());
            callback();

            return;
        }

        QueueCallback data;
        data.Callback = callback;
        data.Label = jobLabel;

        std::lock_guard lock(s_Data->RenderThread.Mutex);
        s_Data->RenderThread.Queue.push(data);
    }

    bool Renderer::Wait(std::optional<std::chrono::milliseconds> timeout) {
        FUUJIN_TRACE("Waiting for render queue to finish...");
        spdlog::stopwatch timer;

        while (true) {
            {
                std::lock_guard lock(s_Data->RenderThread.Mutex);
                if (s_Data->RenderThread.Queue.empty()) {
                    break;
                }
            }

            auto elapsed = timer.elapsed_ms();
            if (timeout.has_value() && elapsed >= timeout.value()) {
                FUUJIN_WARN("Waited {} to clear render queue - timed out", elapsed);
                return false;
            }

            std::this_thread::sleep_for(1ms);
        }

        FUUJIN_TRACE("Waited {} to clear render queue", timer.elapsed_ms());
        return true;
    }

    bool Renderer::IsRenderThread() {
        ZoneScoped;

        return std::this_thread::get_id() == s_Data->RenderThread.ID;
    }

    static void RT_NewFrame(uint32_t frame) {
        ZoneScoped;

        s_Data->API->RT_NewFrame(frame);
    }

    static void RT_BeginRenderTarget(std::shared_ptr<ActiveRenderTarget> target) {
        ZoneScoped;

        if (target->CmdList != nullptr) {
            return; // nothing to do
        }

        target->CmdList = &s_Data->GraphicsQueue->RT_Get();
        target->CmdList->RT_Begin();

        target->Target->RT_BeginRender(*target->CmdList, glm::vec4(glm::vec3(0.1f), 1.f));
    }

    static void RT_EndRenderTarget(std::shared_ptr<ActiveRenderTarget> target) {
        ZoneScoped;

        target->Target->RT_EndRender(*target->CmdList);

        if (target->Target->GetType() == RenderTargetType::Swapchain) {
            s_Data->API->RT_PrePresent(*target->CmdList);
        }

        auto fence = target->Target->GetCurrentFence();
        fence->Reset();

        target->CmdList->RT_End();
        s_Data->GraphicsQueue->RT_Submit(*target->CmdList, fence);

        target->CmdList = nullptr;
        target->Target->RT_EndFrame();
    }

    static void RT_PushRenderTarget(std::shared_ptr<ActiveRenderTarget> target) {
        ZoneScoped;

        if (target->Target->GetType() == RenderTargetType::Swapchain) {
            RT_BeginRenderTarget(target);
        }
    }

    static void RT_PopRenderTarget(std::shared_ptr<ActiveRenderTarget> target) {
        ZoneScoped;

        if (target->CmdList != nullptr) {
            RT_EndRenderTarget(target);
        }
    }

    void Renderer::NewFrame() {
        ZoneScoped;

        if (!s_Data->Targets.empty()) {
            throw std::runtime_error(
                "Must close all render targets before initiating a new frame!");
        }

        uint32_t frame;
        if (s_Data->CurrentFrame.has_value()) {
            frame = s_Data->CurrentFrame.value();

            frame++;
            frame %= s_Data->FrameCount;

            s_Data->CurrentFrame = frame;
        } else {
            s_Data->CurrentFrame = frame = 0;
        }

        Renderer::Submit([frame]() { RT_NewFrame(frame); }, "New frame");
    }

    void Renderer::PushRenderTarget(Ref<RenderTarget> target) {
        ZoneScoped;

        auto newTarget = std::make_shared<ActiveRenderTarget>();
        newTarget->Target = target;
        newTarget->CmdList = nullptr;
        newTarget->ResetViewport = true;

        s_Data->Targets.push(newTarget);
        Renderer::Submit([=]() { RT_PushRenderTarget(newTarget); }, "Push render target");
    }

    void Renderer::PopRenderTarget() {
        ZoneScoped;

        if (s_Data->Targets.empty()) {
            return;
        }

        const auto& target = s_Data->Targets.top();
        Renderer::Submit([target]() { RT_PopRenderTarget(target); }, "Pop render target");

        s_Data->Targets.pop();
    }

    Ref<RenderTarget> Renderer::GetActiveRenderTarget() {
        ZoneScoped;

        if (s_Data->Targets.empty()) {
            return nullptr;
        }

        return s_Data->Targets.top()->Target;
    }

    static void RT_RenderIndexed(IndexedRenderCall* data,
                                 std::shared_ptr<ActiveRenderTarget> target) {
        ZoneScoped;

        RT_BeginRenderTarget(target);

        bool customViewport = data->Flip.has_value() || data->ScissorRect.has_value();
        if (target->ResetViewport || customViewport) {
            s_Data->API->RT_SetViewport(*target->CmdList, target->Target, data->Flip,
                                        data->ScissorRect);

            target->ResetViewport = customViewport;
        }

        s_Data->API->RT_RenderIndexed(*target->CmdList, *data);
        delete data;
    }

    void Renderer::RenderIndexed(const IndexedRenderCall& data) {
        ZoneScoped;

        auto copy = new IndexedRenderCall;
        *copy = data;

        const auto& renderTarget = s_Data->Targets.top();
        Renderer::Submit([copy, renderTarget]() { RT_RenderIndexed(copy, renderTarget); });
    }

    void Renderer::RenderWithMaterial(const MaterialRenderCall& data) {
        ZoneScoped;

        IndexedRenderCall innerCall;
        innerCall.VertexBuffers = data.VertexBuffers;
        innerCall.IndexBuffer = data.IndexBuffer;
        innerCall.RenderPipeline = data.RenderPipeline;
        innerCall.IndexCount = data.IndexCount;

        const auto& shader = data.RenderPipeline->GetSpec().PipelineShader;
        innerCall.Resources = { GetMaterialAllocation(data.RenderMaterial, shader),
                                GetSceneAllocation(data.SceneID, shader) };

        for (const auto& allocation : data.AdditionalResources) {
            innerCall.Resources.push_back(allocation);
        }

        auto pushConstants = shader->GetPushConstants();
        if (pushConstants) {
            ShaderBuffer buffer(pushConstants->GetType());

            // see assets/shaders/include/Renderer.glsl
            buffer.Set("Model", data.ModelMatrix);
            buffer.Set("FirstCamera", (int32_t)data.FirstCamera);
            buffer.Set("CameraCount", (int32_t)data.CameraCount);

            for (const auto& [name, fieldData] : data.PushConstants) {
                buffer.SetData(name, fieldData);
            }

            innerCall.PushConstants = buffer.GetBuffer().Copy();
        } else {
            FUUJIN_WARN("No push constants on shader! Cannot pass model matrix or camera indices!");
        }

        RenderIndexed(innerCall);
    }

    void Renderer::RenderModel(const ModelRenderCall& data) {
        ZoneScoped;

        if (s_Data->Targets.empty()) {
            FUUJIN_ERROR("Attempted to render to an empty target stack!");
            return;
        }

        if (data.ModelAnimator.IsPresent()) {
            data.ModelAnimator->Update();
        }

        const auto& nodes = data.RenderedModel->GetNodes();
        const auto& meshes = data.RenderedModel->GetMeshes();
        const auto& meshNodes = data.RenderedModel->GetMeshNodes();

        auto target = GetActiveRenderTarget();
        for (size_t index : meshNodes) {
            const auto& node = nodes[index];

            for (size_t meshIndex : node.Meshes) {
                const auto& mesh = meshes[meshIndex];

                bool isSkinned = !mesh->GetBones().empty();
                std::string shaderIdentifier = isSkinned ? "fuujin/shaders/MaterialSkinned.glsl"
                                                         : "fuujin/shaders/MaterialStatic.glsl";

                const auto& buffers = GetMeshBuffers(mesh);
                const auto& shader = s_Data->Library->Get(shaderIdentifier);

                const auto& material = mesh->GetMaterial();
                auto pipeline = GetMaterialPipeline(shader, target, material->GetPipeline());

                glm::mat4 nodeTransform(1.f);
                if (data.ModelAnimator.IsPresent()) {
                    const auto& nodeTransforms = data.ModelAnimator->GetNodeTransforms();
                    nodeTransform = nodeTransforms[index];
                } else {
                    std::optional<size_t> currentNode = index;
                    while (currentNode.has_value()) {
                        const auto& currentNodeData = nodes[currentNode.value()];

                        nodeTransform *= currentNodeData.Transform;
                        currentNode = currentNodeData.Parent;
                    }
                }

                MaterialRenderCall innerCall;
                innerCall.VertexBuffers = buffers.VertexBuffers;
                innerCall.IndexBuffer = buffers.IndexBuffer;
                innerCall.IndexCount = (uint32_t)mesh->GetIndices().size();
                innerCall.RenderMaterial = material;
                innerCall.RenderPipeline = pipeline;
                innerCall.SceneID = data.SceneID;
                innerCall.ModelMatrix = data.ModelMatrix * nodeTransform;
                innerCall.FirstCamera = data.FirstCamera;
                innerCall.CameraCount = data.CameraCount;

                if (isSkinned) {
                    if (data.ModelAnimator.IsEmpty()) {
                        FUUJIN_WARN("No animator present on a skinned model! This will cause "
                                    "rendering errors");
                    } else {
                        const auto& boneOffsets = data.ModelAnimator->GetArmatureOffsets();

                        auto allocation = GetAnimatorAllocation(data.ModelAnimator, shader);
                        innerCall.AdditionalResources.push_back(allocation);

                        size_t armature = mesh->GetArmatureIndex();
                        int32_t boneOffset = (int32_t)boneOffsets[armature];

                        innerCall.PushConstants["BoneOffset"] =
                            Buffer::CreateCopy(&boneOffset, sizeof(int32_t));
                    }
                }

                RenderWithMaterial(innerCall);
            }
        }
    }
}; // namespace fuujin
