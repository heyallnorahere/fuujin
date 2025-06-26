#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanRenderer.h"

#include "fuujin/platform/vulkan/VulkanBuffer.h"
#include "fuujin/platform/vulkan/VulkanPipeline.h"
#include "fuujin/platform/vulkan/VulkanCommandQueue.h"
#include "fuujin/platform/vulkan/VulkanContext.h"
#include "fuujin/platform/vulkan/VulkanTexture.h"

namespace fuujin {
    static uint64_t s_CurrentAllocationID = 0;

    VulkanRendererAllocation::VulkanRendererAllocation(const Ref<VulkanShader>& shader) {
        ZoneScoped;

        m_Shader = shader;
        m_ID = s_CurrentAllocationID++;
        m_BindingsChanged = true;
    }

    VulkanRendererAllocation::~VulkanRendererAllocation() {
        ZoneScoped;

        // todo: ?
    }

    bool VulkanRendererAllocation::Bind(const std::string& name, Ref<DeviceBuffer> buffer,
                                        uint32_t index, size_t offset, size_t range) {
        ZoneScoped;

        ResourceDescriptor descriptor;
        if (!m_Shader->GetResourceDescriptor(name, descriptor)) {
            return false;
        }

        auto vulkanBuffer = buffer.As<VulkanBuffer>();
        const auto& spec = vulkanBuffer->GetSpec();

        size_t size = spec.Size;
        size_t bufferRange = range > 0 ? range : size - offset;

        VulkanDescriptor data;
        data.Object = vulkanBuffer;
        data.DescriptorType = vulkanBuffer->GetDescriptorType();

        auto bufferRaw = vulkanBuffer.Raw();
        data.Binder = [=](Buffer& block) {
            auto bufferInfo = (VkDescriptorBufferInfo*)block.Get();
            bufferInfo->buffer = bufferRaw->Get();
            bufferInfo->offset = offset;
            bufferInfo->range = bufferRange;
        };

        SetDescriptor(descriptor.Set, descriptor.Binding, index, data, VulkanBindingType::Buffer);
    }

    bool VulkanRendererAllocation::Bind(const std::string& name, Ref<Texture> texture,
                                        uint32_t index) {
        ZoneScoped;

        ResourceDescriptor descriptor;
        if (!m_Shader->GetResourceDescriptor(name, descriptor)) {
            return false;
        }

        VulkanDescriptor data;
        data.Object = texture;
        data.DescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        auto raw = texture.As<VulkanTexture>().Raw();
        data.Binder = [=](Buffer& block) {
            auto imageInfo = (VkDescriptorImageInfo*)block.Get();
            imageInfo->imageView = raw->GetVulkanImage()->GetView();
            imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            auto sampler = raw->GetSpec().Sampler.As<VulkanSampler>();
            imageInfo->sampler = sampler->GetSampler();
        };

        SetDescriptor(descriptor.Set, descriptor.Binding, index, data, VulkanBindingType::Image);
    }

    struct SetAllocate {
        VkDescriptorSetAllocateInfo AllocInfo;
        uint32_t Offset;
    };

    void VulkanRendererAllocation::RT_AllocateDescriptorSets(const Ref<VulkanDevice>& device,
                                                             VkDescriptorPool pool,
                                                             DescriptorSetArray& sets) {
        ZoneScoped;
        std::lock_guard lock(m_Mutex);

        if (m_BindingsChanged) {
            RecalculateGroups();
        }

        std::vector<VkDescriptorSetLayout> layouts;
        std::map<uint32_t, size_t> offsetMap;

        const auto& shaderLayouts = m_Shader->GetDescriptorSetLayouts();
        for (const auto& [offset, count] : m_SetGroups) {
            offsetMap[offset] = layouts.size();

            for (uint32_t i = 0; i < count; i++) {
                uint32_t setIndex = offset + i;

                if (!shaderLayouts.contains(setIndex)) {
                    throw std::runtime_error("No such set: " + std::to_string(setIndex));
                }

                layouts.push_back(shaderLayouts.at(setIndex));
            }
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = (uint32_t)layouts.size();
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> rawSets(layouts.size());
        if (vkAllocateDescriptorSets(device->GetDevice(), &allocInfo, rawSets.data()) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets for renderer allocation!");
        }

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<Buffer> descriptorInfo(layouts.size());

        for (const auto& [groupOffset, arrayOffset] : offsetMap) {
            uint32_t groupSize = m_SetGroups.at(groupOffset);

            auto begin = rawSets.begin() + arrayOffset;
            auto end = begin + groupSize;

            auto& setGroup = sets[groupOffset];
            setGroup.resize(groupSize);
            std::copy(begin, end, setGroup.begin());

            for (uint32_t i = 0; i < setGroup.size(); i++) {
                uint32_t setIndex = groupOffset + i;
                uint32_t bufferIndex = arrayOffset + i;

                BindSet(setGroup[i], setIndex, writes, descriptorInfo[bufferIndex]);
            }
        }

        if (!writes.empty()) {
            vkUpdateDescriptorSets(device->GetDevice(), (uint32_t)writes.size(), writes.data(), 0,
                                   nullptr);
        }
    }

    bool VulkanRendererAllocation::IsDescriptorCurrent(uint32_t set, uint32_t binding,
                                                       uint32_t index, void* object) const {
        ZoneScoped;

        if (!m_Bindings.contains(set)) {
            return false;
        }

        const auto& setBindings = m_Bindings.at(set);
        if (!setBindings.contains(binding)) {
            return false;
        }

        const auto& bindingData = setBindings.at(binding);
        if (!bindingData.Descriptors.contains(index)) {
            return false;
        }

        const auto& descriptor = bindingData.Descriptors.at(index);
        return object == (void*)descriptor.Object.Raw();
    }

    void VulkanRendererAllocation::SetDescriptor(uint32_t set, uint32_t binding, uint32_t index,
                                                 const VulkanDescriptor& data,
                                                 VulkanBindingType type) {
        ZoneScoped;
        std::lock_guard lock(m_Mutex);

        if (IsDescriptorCurrent(set, binding, index, data.Object.Raw())) {
            return; // we dont need to do anything
        }

        const auto& resources = m_Shader->GetResources();
        const auto& resource = resources.at(set).at(binding);

        auto& setBindings = m_Bindings[set];
        bool bindingExists = setBindings.contains(binding);
        auto& bindingData = setBindings[binding];

        if (!bindingExists) {
            VulkanBindingType bindingType;
            switch (resource.ResourceType) {
            case GPUResource::UniformBuffer:
                bindingType = VulkanBindingType::Buffer;
                break;
            case GPUResource::StorageBuffer:
                bindingType = VulkanBindingType::Buffer;
                break;
            case GPUResource::SampledImage | GPUResource::ShaderSampler:
                bindingType = VulkanBindingType::Image;
                break;
            default:
                throw std::runtime_error("Unimplemented!");
            }

            bindingData.Type = bindingType;
        }

        if (type != bindingData.Type) {
            throw std::runtime_error("Invalid binding type!");
        }

        bindingData.Descriptors[index] = data;
        bindingData.DescriptorsChanged = true;

        m_BindingsChanged = true;
    }

    // solves for groups of keys within the input map
    // intentionally uses std::map for the ordered keys
    template <typename _Key, typename _Val>
    static void CalculateGenericGroups(const std::map<_Key, _Val>& data,
                                       std::map<_Key, _Key>& groups) {
        ZoneScoped;
        static_assert(std::is_integral_v<_Key>, "Key type must be integral!");

        groups.clear();
        for (const auto& [key, data] : data) {
            // the data is an ordered map
            // therefore, we do not need to worry about joining groups

            bool groupFound = false;
            for (auto& [offset, size] : groups) {
                if (offset + size == key) {
                    size++;

                    groupFound = true;
                    break;
                }
            }

            if (groupFound) {
                continue;
            }

            groups[key] = 1;
        }
    }

    void VulkanRendererAllocation::RecalculateGroups() {
        ZoneScoped;
        if (!m_BindingsChanged) {
            return;
        }

        FUUJIN_INFO("Bindings changed on renderer allocation {} - recalculating set groups", m_ID);
        CalculateGenericGroups(m_Bindings, m_SetGroups);

        for (auto& [setIndex, bindings] : m_Bindings) {
            for (auto& [bindingIndex, binding] : bindings) {
                if (!binding.DescriptorsChanged) {
                    continue;
                }

                FUUJIN_DEBUG("Recalculating binding group for binding {}.{} on allocation {}",
                             setIndex, bindingIndex, m_ID);

                CalculateGenericGroups(binding.Descriptors, binding.Groups);

                binding.DescriptorCount = 0;
                binding.OffsetMap.clear();

                for (const auto& [offset, size] : binding.Groups) {
                    binding.OffsetMap[offset] = binding.DescriptorCount;
                    binding.DescriptorCount += size;
                }

                switch (binding.Type) {
                case VulkanBindingType::Buffer:
                    binding.BufferStride = sizeof(VkDescriptorBufferInfo);
                    break;
                case VulkanBindingType::Image:
                    binding.BufferStride = sizeof(VkDescriptorImageInfo);
                    break;
                default:
                    throw std::runtime_error("Unimplemented!");
                }

                binding.BufferSize = binding.BufferStride * binding.DescriptorCount;
                binding.DescriptorsChanged = false;
            }
        }

        m_BindingsChanged = false;
    }

    void VulkanRendererAllocation::BindSet(VkDescriptorSet set, uint32_t index,
                                           std::vector<VkWriteDescriptorSet>& writes,
                                           Buffer& descriptorInfo) const {
        ZoneScoped;

        const auto& setBindings = m_Bindings.at(index);
        const auto& setResources = m_Shader->GetResources().at(index);

        size_t bufferSize = 0;
        std::map<uint32_t, size_t> bufferOffsets;
        for (const auto& [bindingIndex, binding] : setBindings) {
            bufferOffsets[bindingIndex] = bufferSize;
            bufferSize += binding.BufferSize;
        }

        descriptorInfo = Buffer(bufferSize);
        for (const auto& [bindingIndex, binding] : setBindings) {
            const auto& resource = setResources.at(bindingIndex);
            auto descriptorType = VulkanShader::ConvertDescriptorType(resource.ResourceType);

            if (binding.Groups.size() > 1) {
                FUUJIN_WARN(
                    "Resources within descriptor array {}.{} unbound! May cause issues on the GPU",
                    index, bindingIndex);
            }

            for (const auto& [offset, size] : binding.Groups) {
                uint32_t groupOffset =
                    bufferOffsets.at(bindingIndex) + binding.OffsetMap.at(offset);
                auto groupSlice = descriptorInfo.Slice(groupOffset * binding.BufferStride);

                VkWriteDescriptorSet write{};
                write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                write.descriptorType = descriptorType;
                write.dstSet = set;
                write.dstBinding = bindingIndex;
                write.dstArrayElement = offset;
                write.descriptorCount = size;

                switch (binding.Type) {
                case VulkanBindingType::Buffer:
                    write.pBufferInfo = groupSlice.As<VkDescriptorBufferInfo>();
                    break;
                case VulkanBindingType::Image:
                    write.pImageInfo = groupSlice.As<VkDescriptorImageInfo>();
                    break;
                default:
                    throw std::runtime_error("Unimplemented!");
                }

                for (uint32_t i = 0; i < size; i++) {
                    uint32_t arrayIndex = offset + i;
                    const auto& data = binding.Descriptors.at(arrayIndex);

                    if (data.DescriptorType != descriptorType) {
                        FUUJIN_ERROR("Descriptor type mismatch in descriptor {}.{}[{}]!", index,
                                     bindingIndex, arrayIndex);
                    }

                    auto descriptorSlice =
                        groupSlice.Slice(i * binding.BufferStride, binding.BufferStride);

                    data.Binder(descriptorSlice);
                }

                writes.push_back(write);
            }
        }
    }

    VulkanRenderer::VulkanRenderer(Ref<VulkanDevice> device, uint32_t frames) {
        ZoneScoped;

        m_Device = device;
        m_FrameCount = frames;
        m_CurrentFrame = 0;

        Renderer::Submit([this]() { RT_CreatePools(); }, "Create renderer descriptor pools");
    }

    VulkanRenderer::~VulkanRenderer() {
        ZoneScoped;
        Renderer::GetGraphicsQueue()->Clear();

        std::vector<VkDescriptorPool> pools;
        for (const auto& pool : m_DescriptorPools) {
            pools.push_back(pool.DescriptorPool);
        }

        auto device = m_Device->GetDevice();
        Renderer::Submit([=]() {
            auto callbacks = &VulkanContext::GetAllocCallbacks();
            for (auto pool : pools) {
                vkDestroyDescriptorPool(device, pool, callbacks);
            }
        });
    }

    void VulkanRenderer::RT_NewFrame(uint32_t frame) {
        ZoneScoped;

        m_CurrentFrame = frame;
        m_DescriptorPools[frame].MarkedForReset = true;

        auto context = Renderer::GetContext().As<VulkanContext>();
        if (context) {
            m_TracyContext = context->GetTracyContext();
        } else {
            m_TracyContext = nullptr;
        }
    }

    void VulkanRenderer::RT_PrePresent(CommandList& cmdlist) {
        ZoneScoped;
        auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();

        if (m_TracyContext != nullptr) {
            TracyVkCollect(m_TracyContext, cmdBuffer);
        }
    }

    void VulkanRenderer::RT_RenderIndexed(CommandList& cmdlist, const IndexedRenderCall& data) {
        ZoneScoped;

        cmdlist.AddDependency(data.VertexBuffer);
        cmdlist.AddDependency(data.IndexBuffer);
        cmdlist.AddDependency(data.Pipeline);

        auto vertexBuffer = data.VertexBuffer.As<VulkanBuffer>()->Get();
        auto indexBuffer = data.IndexBuffer.As<VulkanBuffer>()->Get();
        auto vkPipeline = data.Pipeline.As<VulkanPipeline>();
        auto shader = vkPipeline->GetSpec().Shader.As<VulkanShader>();
        auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();

        TracyVkZone(m_TracyContext, cmdBuffer, "RT_RenderIndexed");

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->GetPipeline());

        if (data.Resources) {
            RT_BindAllocation(cmdBuffer, data.Resources, VK_PIPELINE_BIND_POINT_GRAPHICS);
        }

        if (data.PushConstants) {
            const auto& shaderData = shader->GetPushConstants();

            VkShaderStageFlags stages = 0;
            for (auto [stage, id] : shaderData.Types) {
                stages |= VulkanShader::ConvertStage(stage);
            }

            if (stages != 0) {
                vkCmdPushConstants(cmdBuffer, shader->GetPipelineLayout(), stages, 0,
                                   (uint32_t)data.PushConstants.GetSize(),
                                   data.PushConstants.Get());
            }
        }

        vkCmdDrawIndexed(cmdBuffer, data.IndexCount, 1, 0, 0, 0);
    }

    void VulkanRenderer::RT_SetViewport(CommandList& cmdlist, Ref<RenderTarget> target) const {
        ZoneScoped;

        uint32_t width = target->GetWidth();
        uint32_t height = target->GetHeight();

        VkRect2D scissor{};
        scissor.extent.width = width;
        scissor.extent.height = height;

        VkViewport viewport{};
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        switch (target->GetType()) {
        case RenderTargetType::Swapchain:
            viewport.x = 0.f;
            viewport.y = (float)height;
            viewport.width = (float)width;
            viewport.height = -(float)height;
            break;
        case RenderTargetType::Framebuffer:
            viewport.x = 0.f;
            viewport.y = 0.f;
            viewport.width = (float)width;
            viewport.height = (float)height;
            break;
        default:
            throw std::runtime_error("Invalid render target type!");
        }

        auto cmdBuffer = ((VulkanCommandBuffer&)cmdlist).Get();
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    }

    Ref<RendererAllocation> VulkanRenderer::CreateAllocation(const Ref<Shader>& shader) const {
        ZoneScoped;

        return Ref<VulkanRendererAllocation>::Create(shader.As<VulkanShader>());
    }

    void VulkanRenderer::RT_CreatePools() {
        ZoneScoped;

        static const std::vector<VkDescriptorPoolSize> poolSizes = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
        };

        VkDescriptorPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.maxSets = 10;
        createInfo.poolSizeCount = (uint32_t)poolSizes.size();
        createInfo.pPoolSizes = poolSizes.data();

        auto callbacks = &VulkanContext::GetAllocCallbacks();
        m_DescriptorPools.resize(m_FrameCount);
        for (auto& pool : m_DescriptorPools) {
            pool.MarkedForReset = false;

            if (vkCreateDescriptorPool(m_Device->GetDevice(), &createInfo, callbacks,
                                       &pool.DescriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create descriptor pool!");
            }
        }
    }

    void VulkanRenderer::RT_ResetCurrentPool() {
        ZoneScoped;

        auto& pool = m_DescriptorPools[m_CurrentFrame];
        if (!pool.MarkedForReset) {
            return;
        }

        vkResetDescriptorPool(m_Device->GetDevice(), pool.DescriptorPool, 0);

        pool.Allocations.clear();
        pool.MarkedForReset = false;
    }

    void VulkanRenderer::RT_BindAllocation(VkCommandBuffer cmdBuffer,
                                           Ref<RendererAllocation> allocation,
                                           VkPipelineBindPoint bindPoint) {
        ZoneScoped;
        TracyVkZone(m_TracyContext, cmdBuffer, "RT_BindAllocation");

        RT_ResetCurrentPool();

        auto rendererAlloc = allocation.As<VulkanRendererAllocation>();
        uint64_t allocID = rendererAlloc->GetID();

        auto& pool = m_DescriptorPools[m_CurrentFrame];
        if (!pool.Allocations.contains(allocID)) {
            auto& allocData = pool.Allocations[allocID];

            // for ref counting
            allocData.Allocation = rendererAlloc;
            allocData.Bindings = rendererAlloc->GetBindings(); // intentionally copy

            rendererAlloc->RT_AllocateDescriptorSets(m_Device, pool.DescriptorPool, allocData.Sets);
        }

        const auto& setGroups = pool.Allocations.at(allocID).Sets;
        auto pipelineLayout = rendererAlloc->GetShader()->GetPipelineLayout();

        for (const auto& [firstSet, sets] : setGroups) {
            vkCmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout, firstSet,
                                    (uint32_t)sets.size(), sets.data(), 0, nullptr);
        }
    }
} // namespace fuujin