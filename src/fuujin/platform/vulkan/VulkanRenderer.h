#pragma once
#include "fuujin/renderer/Renderer.h"

#include "fuujin/platform/vulkan/VulkanDevice.h"
#include "fuujin/platform/vulkan/VulkanShader.h"

namespace fuujin {
    struct VulkanDescriptor {
        Ref<RefCounted> Object;
        VkDescriptorType DescriptorType;
        std::function<void(Buffer&)> Binder;
    };

    enum class VulkanBindingType { Buffer, Image };
    struct VulkanBinding {
        bool DescriptorsChanged;
        std::map<uint32_t, uint32_t> Groups;

        uint32_t DescriptorCount;
        std::map<uint32_t, uint32_t> OffsetMap;
        size_t BufferSize, BufferStride;

        std::map<uint32_t, VulkanDescriptor> Descriptors;
        VulkanBindingType Type;
    };

    class VulkanRendererAllocation : public RendererAllocation {
    public:
        using DescriptorSetArray = std::map<uint32_t, std::vector<VkDescriptorSet>>;
        using Bindings = std::map<uint32_t, std::map<uint32_t, VulkanBinding>>;

        VulkanRendererAllocation(const Ref<VulkanShader>& shader);
        ~VulkanRendererAllocation();

        Ref<VulkanShader> GetShader() const { return m_Shader; }
        uint64_t GetID() const { return m_ID; }
        const Bindings& GetBindings() const { return m_Bindings; }

        virtual bool Bind(const std::string& name, Ref<DeviceBuffer> buffer, uint32_t index = 0,
                          size_t offset = 0, size_t range = 0) override;

        void RT_AllocateDescriptorSets(const Ref<VulkanDevice>& device, VkDescriptorPool pool,
                                       DescriptorSetArray& sets);

    private:
        bool IsDescriptorCurrent(uint32_t set, uint32_t binding, uint32_t index,
                                 void* object) const;

        void SetDescriptor(uint32_t set, uint32_t binding, uint32_t index,
                           const VulkanDescriptor& data, VulkanBindingType type);

        void RecalculateGroups();

        void BindSet(VkDescriptorSet set, uint32_t index, std::vector<VkWriteDescriptorSet>& writes,
                     Buffer& descriptorInfo) const;

        Ref<VulkanShader> m_Shader;

        uint64_t m_ID;
        Bindings m_Bindings;

        bool m_BindingsChanged;
        std::map<uint32_t, uint32_t> m_SetGroups;

        std::mutex m_Mutex;
    };

    class VulkanRenderer : public RendererAPI {
    public:
        struct FrameAllocationData {
            Ref<VulkanRendererAllocation> Allocation;
            VulkanRendererAllocation::Bindings Bindings; // we keep references until frame reset
            VulkanRendererAllocation::DescriptorSetArray Sets;
        };

        struct DescriptorPool {
            VkDescriptorPool DescriptorPool;
            bool MarkedForReset;

            std::unordered_map<uint64_t, FrameAllocationData> Allocations;
        };

        VulkanRenderer(Ref<VulkanDevice> device, uint32_t frames);
        ~VulkanRenderer();

        virtual void RT_NewFrame(uint32_t frame) override;

        virtual void RT_RenderIndexed(CommandList& cmdlist, const IndexedRenderCall& data) override;

        virtual void RT_SetViewport(CommandList& cmdlist, Ref<RenderTarget> target) const override;

        virtual Ref<RendererAllocation> CreateAllocation(const Ref<Shader>& shader) const override;

    private:
        void RT_CreatePools();
        void RT_ResetCurrentPool();

        void RT_BindAllocation(VkCommandBuffer cmdBuffer, Ref<RendererAllocation> allocation,
                               VkPipelineBindPoint bindPoint);

        Ref<VulkanDevice> m_Device;
        uint32_t m_FrameCount;

        uint32_t m_CurrentFrame;
        std::vector<DescriptorPool> m_DescriptorPools;
    };
} // namespace fuujin