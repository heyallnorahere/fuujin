#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanSwapchain.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

#include "fuujin/core/Events.h"

namespace fuujin {
    static void ClearGraphicsQueue() {
        ZoneScoped;

        auto context = VulkanContext::Get();
        auto queue = context->GetQueue(QueueType::Graphics);
        queue->Clear();
    }

    VulkanSwapchain::VulkanSwapchain(Ref<View> view, Ref<VulkanDevice> device)
        : m_View(view), m_Device(device), m_ImageFormat(VK_FORMAT_UNDEFINED), m_Extent({}),
          m_SyncFrame(0) {
        ZoneScoped;

        m_Swapchain = VK_NULL_HANDLE;
        Renderer::Submit(
            [this]() mutable {
                m_Surface = (VkSurfaceKHR)m_View->CreateVulkanSurface(
                    m_Device->GetInstance()->GetInstance());
            },
            "Create surface");
    }

    VulkanSwapchain::~VulkanSwapchain() {
        ZoneScoped;

        m_Framebuffers.clear();

        auto device = m_Device->GetDevice();
        auto instance = m_Device->GetInstance()->GetInstance();
        auto swapchain = m_Swapchain;
        auto surface = m_Surface;

        Renderer::Submit(
            [device, instance, swapchain, surface]() {
                if (swapchain != VK_NULL_HANDLE) {
                    vkDestroySwapchainKHR(device, swapchain, &VulkanContext::GetAllocCallbacks());
                }

                if (surface != VK_NULL_HANDLE) {
                    vkDestroySurfaceKHR(instance, surface, &VulkanContext::GetAllocCallbacks());
                }
            },
            "Destroy swapchain");
    }

    std::optional<uint32_t> VulkanSwapchain::RT_FindDeviceQueue() {
        ZoneScoped;

        if (!m_PresentFamily.has_value() && m_Surface != nullptr) {
            auto physicalDevice = m_Device->GetPhysicalDevice();
            uint32_t queueCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);

            for (uint32_t i = 0; i < queueCount; i++) {
                VkBool32 supported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_Surface, &supported);

                if (supported == VK_TRUE) {
                    m_PresentFamily = i;
                    break;
                }
            }
        }

        return m_PresentFamily;
    }

    void VulkanSwapchain::RT_Initialize(VmaAllocator allocator) {
        ZoneScoped;
        m_Allocator = allocator;

        auto queue = RT_FindDeviceQueue();
        if (queue.has_value()) {
            vkGetDeviceQueue(m_Device->GetDevice(), queue.value(), 0, &m_PresentQueue);

            RT_Invalidate();
        }
    }

    void VulkanSwapchain::RequestResize(const ViewSize& viewSize) {
        ZoneScoped;
        m_NewViewSize = viewSize;

        FUUJIN_INFO("Requested swapchain resize from {}x{} to {}x{}", m_Extent.width,
                    m_Extent.height, viewSize.Width, viewSize.Height);
    }

    void VulkanSwapchain::RT_BeginRender(CommandList& cmdList, const glm::vec4& clearColor) {
        ZoneScoped;
        RT_AcquireImage();

        auto buffer = (VulkanCommandBuffer*)&cmdList;
        auto framebuffer = m_Framebuffers[m_CurrentImage];
        framebuffer->RT_BeginRenderPass(buffer, clearColor);

        auto semaphore = m_ImageSemaphores[m_CurrentImage];
        buffer->AddWaitSemaphore(semaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    void VulkanSwapchain::RT_EndRender(CommandList& cmdList) {
        ZoneScoped;

        auto buffer = (VulkanCommandBuffer*)&cmdList;
        auto framebuffer = m_Framebuffers[m_CurrentImage];
        framebuffer->RT_EndRenderPass(buffer);

        auto semaphore = m_PresentationSemaphores[m_CurrentImage];
        buffer->AddSemaphore(semaphore, SemaphoreUsage::Signal);
    }

    void VulkanSwapchain::RT_EndFrame() {
        ZoneScoped;
        RT_Present();

        m_SyncFrame = (m_SyncFrame + 1) % m_Framebuffers.size();
    }

    void VulkanSwapchain::RT_AcquireImage() {
        ZoneScoped;

        Ref<VulkanSemaphore> imageAvailable;
        if (m_AvailableSemaphores.empty()) {
            imageAvailable = Ref<VulkanSemaphore>::Create(m_Device);
        } else {
            imageAvailable = m_AvailableSemaphores.front();
            m_AvailableSemaphores.pop();
        }

        while (true) {
            const auto& sync = m_Sync[m_SyncFrame];
            auto fence = sync.Fence;

            fence->Wait(std::numeric_limits<uint64_t>::max());

            auto result = vkAcquireNextImageKHR(
                m_Device->GetDevice(), m_Swapchain, std::numeric_limits<uint64_t>::max(),
                imageAvailable->Get(), VK_NULL_HANDLE, &m_CurrentImage);

            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                RT_Invalidate();
                continue;
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to acquire swapchain image!");
            }

            break;
        }

        auto previousSemapohre = m_ImageSemaphores[m_CurrentImage];
        if (previousSemapohre.IsPresent()) {
            m_AvailableSemaphores.push(previousSemapohre);
        }

        imageAvailable->SetSignaled();
        m_ImageSemaphores[m_CurrentImage] = imageAvailable;
    }

    void VulkanSwapchain::RT_Present() {
        ZoneScoped;

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        VkSemaphore waitSemaphore = m_PresentationSemaphores[m_CurrentImage]->Get();
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &waitSemaphore;

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &m_CurrentImage;

        VkResult result;
        {
            ZoneScopedN("vkQueuePresentKHR");
            result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
            m_NewViewSize.has_value()) {
            RT_Invalidate();
            m_NewViewSize.reset();
        }
    }

    void VulkanSwapchain::RT_Invalidate() {
        ZoneScoped;

        ClearGraphicsQueue();
        m_Framebuffers.clear();

        auto device = m_Device->GetDevice();
        auto old = RT_Create();

        if (old != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, old, &VulkanContext::GetAllocCallbacks());
        }

        auto samples = m_Device->RT_GetMaxSampleCount();
        RT_CreateColorBuffer(samples);
        RT_CreateDepthBuffer(samples);

        if (m_RenderPass.IsEmpty()) {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = m_ImageFormat;
            colorAttachment.samples = samples;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentDescription depthAttachment{};
            depthAttachment.format = m_Depth->GetSpec().Format;
            depthAttachment.samples = samples;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentDescription resolveAttachment{};
            resolveAttachment.format = m_ImageFormat;
            resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference depthAttachmentRef{};
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentReference resolveAttachmentRef{};
            resolveAttachmentRef.attachment = 2;
            resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
            subpass.pResolveAttachments = &resolveAttachmentRef;

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment,
                                                                 resolveAttachment };

            VkRenderPassCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            createInfo.attachmentCount = (uint32_t)attachments.size();
            createInfo.pAttachments = attachments.data();
            createInfo.subpassCount = 1;
            createInfo.pSubpasses = &subpass;
            createInfo.dependencyCount = 1;
            createInfo.pDependencies = &dependency;

            m_RenderPass = Ref<VulkanRenderPass>::Create(m_Device, &createInfo);
        }

        if (m_Swapchain != VK_NULL_HANDLE) {
            uint32_t imageCount;
            vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);

            std::vector<VkImage> images(imageCount);
            vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, images.data());

            for (uint32_t i = 0; i < imageCount; i++) {
                VulkanImage::VulkanSpec viewSpec;
                viewSpec.ExistingImage = images[i];
                viewSpec.Extent.width = m_Extent.width;
                viewSpec.Extent.height = m_Extent.height;
                viewSpec.Extent.depth = 1;
                viewSpec.Format = m_ImageFormat;
                viewSpec.ViewType = VK_IMAGE_VIEW_TYPE_2D;
                viewSpec.AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
                viewSpec.ArrayLayers = 1;
                viewSpec.MipLevels = 1;
                viewSpec.Samples = VK_SAMPLE_COUNT_1_BIT;

                VulkanFramebuffer::VulkanSpec framebufferSpec;
                framebufferSpec.RenderPass = m_RenderPass;
                framebufferSpec.Extent = m_Extent;
                framebufferSpec.Attachments = { m_Color, m_Depth,
                                                Ref<VulkanImage>::Create(m_Device, viewSpec) };

                m_Framebuffers.push_back(Ref<VulkanFramebuffer>::Create(m_Device, framebufferSpec));
            }
        }

        m_SyncFrame %= m_Framebuffers.size();
        m_ImageSemaphores.resize(m_Framebuffers.size());

        while (m_PresentationSemaphores.size() < m_Framebuffers.size()) {
            m_PresentationSemaphores.push_back(Ref<VulkanSemaphore>::Create(m_Device));
        }

        if (m_Sync.size() != m_Framebuffers.size()) {
            m_Sync.resize(m_Framebuffers.size());
        }

        for (size_t i = 0; i < m_Sync.size(); i++) {
            auto& frameSync = m_Sync[i];

            if (frameSync.Fence.IsEmpty()) {
                frameSync.Fence = Ref<VulkanFence>::Create(m_Device, true);
            }
        }
    }

    VkSwapchainKHR VulkanSwapchain::RT_Create() {
        ZoneScoped;

        VkSurfaceCapabilitiesKHR capabilities{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Device->GetPhysicalDevice(), m_Surface,
                                                      &capabilities) != VK_SUCCESS) {
            throw std::runtime_error("Failed to retrieve surface capabilities!");
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0) {
            imageCount = std::min(imageCount, capabilities.maxImageCount);
        }

        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            m_Extent = capabilities.currentExtent;
        } else {
            ViewSize currentSize;
            if (m_NewViewSize.has_value()) {
                currentSize = m_NewViewSize.value();
            } else {
                currentSize = m_View->GetFramebufferSize();
            }

            m_Extent.width = std::clamp(currentSize.Width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);

            m_Extent.height = std::clamp(currentSize.Height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);
        }

        auto presentMode = RT_FindPresentMode();
        auto surfaceFormat = RT_FindSurfaceFormat();
        m_ImageFormat = surfaceFormat.format;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.pNext = nullptr; // do we need anything here?
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Surface;
        createInfo.oldSwapchain = m_Swapchain;
        createInfo.presentMode = presentMode;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = m_Extent;
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        std::unordered_set<uint32_t> imageQueues;
        auto presentQueue = RT_FindDeviceQueue();
        if (presentQueue.has_value()) {
            imageQueues.insert(presentQueue.value());
        }

        auto deviceQueues = m_Device->GetQueues();
        if (deviceQueues.contains(QueueType::Graphics)) {
            imageQueues.insert(deviceQueues[QueueType::Graphics]);
        }

        std::vector<uint32_t> queueIndices;
        if (imageQueues.size() > 1) {
            queueIndices.insert(queueIndices.end(), imageQueues.begin(), imageQueues.end());

            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.pQueueFamilyIndices = queueIndices.data();
            createInfo.queueFamilyIndexCount = (uint32_t)queueIndices.size();
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        VkResult result = vkCreateSwapchainKHR(m_Device->GetDevice(), &createInfo,
                                               &VulkanContext::GetAllocCallbacks(), &m_Swapchain);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain!");
        }

        FUUJIN_INFO("Swapchain {} at size {}x{}",
                    createInfo.oldSwapchain != VK_NULL_HANDLE ? "recreated" : "created",
                    m_Extent.width, m_Extent.height);

        return createInfo.oldSwapchain;
    }

    VkSurfaceFormatKHR VulkanSwapchain::RT_FindSurfaceFormat() const {
        ZoneScoped;
        auto physicalDevice = m_Device->GetPhysicalDevice();

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount,
                                             formats.data());

        static constexpr VkColorSpaceKHR preferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        static const std::vector<VkFormat> preferredFormats = { VK_FORMAT_B8G8R8A8_UNORM,
                                                                VK_FORMAT_R8G8B8A8_UNORM,
                                                                VK_FORMAT_B8G8R8_UNORM,
                                                                VK_FORMAT_R8G8B8_UNORM };

        if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
            VkSurfaceFormatKHR preferred;
            preferred.format = preferredFormats[0];
            preferred.colorSpace = preferredColorSpace;

            return preferred;
        }

        for (auto preferredFormat : preferredFormats) {
            for (const auto& format : formats) {
                if (format.format == preferredFormat && format.colorSpace == preferredColorSpace) {
                    return format;
                }
            }
        }

        return formats[0];
    }

    VkPresentModeKHR VulkanSwapchain::RT_FindPresentMode() const {
        ZoneScoped;

        static constexpr bool vsync = false; // todo: take in parameter from... somewhere
        if (!vsync) {
            auto physicalDevice = m_Device->GetPhysicalDevice();

            uint32_t modeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &modeCount,
                                                      nullptr);

            std::vector<VkPresentModeKHR> presentModes(modeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &modeCount,
                                                      presentModes.data());

            static constexpr VkPresentModeKHR preferredMode = VK_PRESENT_MODE_MAILBOX_KHR;
            for (auto mode : presentModes) {
                if (mode == preferredMode) {
                    return mode;
                }
            }
        }

        // always supported, vsync
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // accesses m_ImageFormat
    // use after RT_Create
    void VulkanSwapchain::RT_CreateColorBuffer(VkSampleCountFlagBits samples) {
        ZoneScoped;

        VulkanImage::VulkanSpec spec;
        spec.Format = m_ImageFormat;
        spec.Allocator = m_Allocator;
        spec.Samples = samples;
        spec.Type = VK_IMAGE_TYPE_2D;
        spec.ViewType = VK_IMAGE_VIEW_TYPE_2D;
        spec.Extent.width = m_Extent.width;
        spec.Extent.height = m_Extent.height;
        spec.Extent.depth = 1;
        spec.QueueOwnership.insert(QueueType::Graphics);
        spec.Usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        m_Color = Ref<VulkanImage>::Create(m_Device, spec);
    }

    void VulkanSwapchain::RT_CreateDepthBuffer(VkSampleCountFlagBits samples) {
        ZoneScoped;

        static constexpr VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        static const std::unordered_map<VkFormat, bool> formatCandidates = {
            { VK_FORMAT_D32_SFLOAT, false },
            { VK_FORMAT_D32_SFLOAT_S8_UINT, true },
            { VK_FORMAT_D24_UNORM_S8_UINT, true }
        };

        std::vector<VkFormat> formats;
        for (const auto& [format, hasStencil] : formatCandidates) {
            formats.push_back(format);
        }

        auto discoveredFormat = m_Device->RT_FindSupportedFormat(
            formats, tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        if (!discoveredFormat.has_value()) {
            throw std::runtime_error("No depth attachment format available!");
        }

        VulkanImage::VulkanSpec spec;
        spec.Extent.width = m_Extent.width;
        spec.Extent.height = m_Extent.height;
        spec.Extent.depth = 1;
        spec.Format = discoveredFormat.value();
        spec.Tiling = tiling;
        spec.Type = VK_IMAGE_TYPE_2D;
        spec.ViewType = VK_IMAGE_VIEW_TYPE_2D;
        spec.Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        spec.Allocator = m_Allocator;
        spec.Samples = samples;
        spec.AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

        m_Depth = Ref<VulkanImage>::Create(m_Device, spec);
        m_DepthHasStencil = formatCandidates.at(spec.Format);
    }
} // namespace fuujin
