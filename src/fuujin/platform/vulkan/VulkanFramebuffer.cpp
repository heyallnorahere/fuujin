#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanFramebuffer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    static Ref<VulkanRenderPass> CreateFramebufferRenderPass(const Ref<VulkanDevice>& device,
                                                             const Framebuffer::Spec& spec,
                                                             VkSampleCountFlagBits samples) {
        ZoneScoped;

        VkPipelineStageFlags stageMask = 0;
        VkAccessFlags dstAccessMask = 0;

        for (size_t i = 0; i < spec.Attachments.size(); i++) {
            const auto& attachment = spec.Attachments[i];

            switch (attachment.Type) {
            case Framebuffer::AttachmentType::Color:
                stageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                break;
            case Framebuffer::AttachmentType::Depth:
                stageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                break;
            default:
                FUUJIN_ERROR("Invalid attachment type!");
                return nullptr;
            }
        }

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = stageMask;
        dependency.dstStageMask = stageMask;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = dstAccessMask;

        std::set<size_t> resolveAttachmentIndices;
        for (size_t i = 0; i < spec.Attachments.size(); i++) {
            if (spec.Attachments[i].Type != Framebuffer::AttachmentType::Color) {
                continue;
            }

            if (spec.ResolveAttachments.contains(i)) {
                resolveAttachmentIndices.insert(spec.ResolveAttachments.at(i));
            }
        }

        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorAttachments, resolveAttachments;
        std::unique_ptr<VkAttachmentReference> depthAttachment;

        for (size_t i = 0; i < spec.Attachments.size(); i++) {
            const auto& attachment = spec.Attachments[i];

            auto& desc = attachments.emplace_back();
            desc.format = VulkanTexture::QueryFormat(attachment.Format).VulkanFormat;
            desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // these are textures
            desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

            if (!resolveAttachmentIndices.contains(i)) {
                if (attachment.ImageType != Texture::Type::_2D) {
                    desc.samples = VK_SAMPLE_COUNT_1_BIT;
                } else {
                    desc.samples = samples;
                }

                desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            } else {
                desc.samples = VK_SAMPLE_COUNT_1_BIT;
                desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

                continue;
            }

            switch (attachment.Type) {
            case Framebuffer::AttachmentType::Color: {
                auto& mainRef = colorAttachments.emplace_back();
                mainRef.attachment = (uint32_t)i;
                mainRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                if (!spec.ResolveAttachments.empty()) {
                    auto& resolveRef = resolveAttachments.emplace_back();
                    resolveRef.layout = mainRef.layout;

                    if (spec.ResolveAttachments.contains(i)) {
                        resolveRef.attachment = (uint32_t)spec.ResolveAttachments.at(i);
                    } else {
                        resolveRef.attachment = VK_ATTACHMENT_UNUSED;
                    }
                }
            } break;
            case Framebuffer::AttachmentType::Depth:
                if (depthAttachment) {
                    FUUJIN_ERROR("Cannot use more than one depth attachment per render pass!");
                    return nullptr;
                }

                depthAttachment = std::make_unique<VkAttachmentReference>();
                depthAttachment->attachment = (uint32_t)i;
                depthAttachment->layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                break;
            default:
                FUUJIN_ERROR("Invalid attachment type!");
                break;
            }
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = depthAttachment.get();

        if (!colorAttachments.empty()) {
            subpass.colorAttachmentCount = (uint32_t)colorAttachments.size();
            subpass.pColorAttachments = colorAttachments.data();

            if (!resolveAttachments.empty()) {
                subpass.pResolveAttachments = resolveAttachments.data();
            }
        }

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = (uint32_t)attachments.size();
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;

        return Ref<VulkanRenderPass>::Create(device, &createInfo);
    }

    VulkanFramebuffer::VulkanFramebuffer(const Ref<VulkanDevice>& device, const Spec& spec,
                                         const VmaAllocator& allocator,
                                         const Ref<VulkanRenderPass>& renderPass) {
        ZoneScoped;

        m_Allocator = &allocator;
        m_Device = device;
        m_Spec = spec;

        m_VulkanSpec.Layers = spec.Layers;
        m_VulkanSpec.Extent.width = m_Spec.Width;
        m_VulkanSpec.Extent.height = m_Spec.Height;

        VkSampleCountFlagBits samples;
        Renderer::Submit([&]() { samples = device->RT_GetMaxSampleCount(); });
        Renderer::Wait();

        if (renderPass.IsPresent()) {
            m_VulkanSpec.RenderPass = renderPass;
        } else {
            m_VulkanSpec.RenderPass = CreateFramebufferRenderPass(device, m_Spec, samples);
        }

        std::set<size_t> resolveAttachments;
        for (const auto& [color, resolve] : m_Spec.ResolveAttachments) {
            resolveAttachments.insert(resolve);
        }

        for (size_t i = 0; i < m_Spec.Attachments.size(); i++) {
            const auto& attachment = m_Spec.Attachments[i];

            Texture::Feature attachmentFeature;
            switch (attachment.Type) {
            case AttachmentType::Color:
                attachmentFeature = Texture::Feature::ColorAttachment;
                break;
            case AttachmentType::Depth:
                attachmentFeature = Texture::Feature::DepthAttachment;
                break;
            default:
                throw std::runtime_error("Invalid attachment type!");
            }

            Texture::Spec spec;
            spec.Width = m_Spec.Width;
            spec.Height = m_Spec.Height;
            spec.Depth = 1;
            spec.MipLevels = 1;

            spec.AdditionalFeatures.insert(attachmentFeature);
            spec.AdditionalFeatures.insert(m_Spec.AttachmentFeatures.begin(),
                                           m_Spec.AttachmentFeatures.end());

            spec.ImageFormat = attachment.Format;
            spec.TextureType = attachment.ImageType;
            spec.TextureSampler = attachment.TextureSampler;

            if (resolveAttachments.contains(i) || attachment.ImageType != Texture::Type::_2D) {
                spec.Samples = 1;
            } else {
                spec.Samples = (uint32_t)samples;
            }

            auto texture = Ref<VulkanTexture>::Create(m_Device, allocator, spec);
            m_Textures.push_back(texture);
        }

        Renderer::Submit(
            [&]() {
                for (const auto& texture : m_Textures) {
                    m_VulkanSpec.Attachments.push_back(texture->GetVulkanImage());
                }

                RT_Create();
            },
            "Create framebuffer");
    }

    VulkanFramebuffer::VulkanFramebuffer(const Ref<VulkanDevice>& device, const VulkanSpec& spec) {
        ZoneScoped;

        m_Allocator = nullptr;
        m_Device = device;
        m_VulkanSpec = spec;

        Renderer::Submit([&]() { RT_Create(); }, "Create framebuffer");
    }

    VulkanFramebuffer::~VulkanFramebuffer() {
        ZoneScoped;

        auto device = m_Device->GetDevice();
        auto framebuffer = m_Framebuffer;

        Renderer::Submit(
            [device, framebuffer]() {
                vkDestroyFramebuffer(device, framebuffer, &VulkanContext::GetAllocCallbacks());
            },
            "Destroy framebuffer");
    }

    void VulkanFramebuffer::RT_BeginRender(CommandList& cmdList, const glm::vec4& clearColor) {
        ZoneScoped;

        auto buffer = (VulkanCommandBuffer*)&cmdList;
        RT_BeginRenderPass(buffer, clearColor);
    }

    void VulkanFramebuffer::RT_EndRender(CommandList& cmdList) {
        ZoneScoped;

        auto buffer = (VulkanCommandBuffer*)&cmdList;
        RT_EndRenderPass(buffer);

        for (const auto& image : m_VulkanSpec.Attachments) {
            auto semaphore = image->GetSignaledSemaphore();
            if (!semaphore.IsEmpty()) {
                buffer->AddWaitSemaphore(semaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
            }

            semaphore = image->SignalUsed();
            cmdList.AddSemaphore(semaphore, SemaphoreUsage::Signal);
        }
    }

    void VulkanFramebuffer::RT_BeginRenderPass(VulkanCommandBuffer* buffer,
                                               const glm::vec4& clear) const {
        ZoneScoped;

        std::vector<VkClearValue> clearValues;
        if (m_Spec.Attachments.empty()) {
            clearValues.resize(2, {});

            std::memcpy(clearValues[0].color.float32, &clear.x, sizeof(float) * 4);
            clearValues[1].depthStencil = { 1.f, 0 };
        } else {
            for (const auto& attachment : m_Spec.Attachments) {
                VkClearValue value{};

                switch (attachment.Type) {
                case AttachmentType::Color:
                    std::memcpy(value.color.float32, &clear.x, sizeof(float) * 4);
                    break;
                case AttachmentType::Depth:
                    value.depthStencil = { 1.f, 0 };
                    break;
                }

                clearValues.push_back(value);
            }
        }

        VkRenderPassBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.clearValueCount = (uint32_t)clearValues.size();
        beginInfo.pClearValues = clearValues.data();
        beginInfo.framebuffer = m_Framebuffer;
        beginInfo.renderPass = m_VulkanSpec.RenderPass->Get();
        beginInfo.renderArea.extent = m_VulkanSpec.Extent;

        vkCmdBeginRenderPass(buffer->Get(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void VulkanFramebuffer::RT_EndRenderPass(VulkanCommandBuffer* buffer) const {
        ZoneScoped;

        vkCmdEndRenderPass(buffer->Get());
    }

    Ref<Texture> VulkanFramebuffer::GetTexture(size_t index) const {
        ZoneScoped;

        if (index >= m_Textures.size()) {
            return nullptr;
        }

        return m_Textures[index];
    }

    Ref<Framebuffer> VulkanFramebuffer::Recreate(uint32_t width, uint32_t height) const {
        ZoneScoped;

        Spec newSpec = m_Spec;
        newSpec.Width = width;
        newSpec.Height = height;

        return Ref<VulkanFramebuffer>::Create(m_Device, newSpec, *m_Allocator,
                                              m_VulkanSpec.RenderPass);
    }

    void VulkanFramebuffer::RT_Create() {
        ZoneScoped;

        std::vector<VkImageView> views;
        for (auto image : m_VulkanSpec.Attachments) {
            views.push_back(image->GetView());
        }

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.flags = m_VulkanSpec.Flags;
        createInfo.renderPass = m_VulkanSpec.RenderPass->Get();
        createInfo.attachmentCount = (uint32_t)views.size();
        createInfo.pAttachments = views.data();
        createInfo.width = m_VulkanSpec.Extent.width;
        createInfo.height = m_VulkanSpec.Extent.height;
        createInfo.layers = m_Spec.Layers;

        if (vkCreateFramebuffer(m_Device->GetDevice(), &createInfo,
                                &VulkanContext::GetAllocCallbacks(),
                                &m_Framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
} // namespace fuujin
