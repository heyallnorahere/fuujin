#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanSwapchain.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

#include "fuujin/core/Events.h"

namespace fuujin {
    VulkanSwapchain::VulkanSwapchain(Ref<View> view, Ref<VulkanDevice> device)
        : m_View(view), m_Device(device), m_ImageFormat(VK_FORMAT_UNDEFINED), m_Extent({}) {
        ZoneScoped;

        m_Swapchain = VK_NULL_HANDLE;
        Renderer::Submit([this]() mutable {
            m_Surface =
                (VkSurfaceKHR)m_View->CreateVulkanSurface(m_Device->GetInstance()->GetInstance());
        });
    }

    VulkanSwapchain::~VulkanSwapchain() {
        ZoneScoped;

        // todo: destroy framebuffers

        auto device = m_Device->GetDevice();
        auto instance = m_Device->GetInstance()->GetInstance();
        auto swapchain = m_Swapchain;
        auto surface = m_Surface;

        Renderer::Submit([device, instance, swapchain, surface]() {
            if (swapchain != VK_NULL_HANDLE) {
                vkDestroySwapchainKHR(device, swapchain, &VulkanContext::GetAllocCallbacks());
            }

            if (surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(instance, surface, &VulkanContext::GetAllocCallbacks());
            }
        });
    }

    std::optional<uint32_t> VulkanSwapchain::RT_FindDeviceQueue() {
        ZoneScoped;

        if (!m_RenderQueue.has_value() && m_Surface != nullptr) {
            auto physicalDevice = m_Device->GetPhysicalDevice();
            uint32_t queueCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);

            for (uint32_t i = 0; i < queueCount; i++) {
                VkBool32 supported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_Surface, &supported);

                if (supported == VK_TRUE) {
                    m_RenderQueue = i;
                    break;
                }
            }
        }

        return m_RenderQueue;
    }

    void VulkanSwapchain::Initialize(const VmaAllocator& allocator) {
        ZoneScoped;
        Renderer::Submit([&]() {
            m_Allocator = allocator; // :thumbsup:

            auto queue = RT_FindDeviceQueue();
            if (queue.has_value()) {
                RT_Invalidate();
            }
        });
    }

    void VulkanSwapchain::ProcessEvent(Event& event) {
        if (event.GetType() == EventType::FramebufferResized) {
            m_NewViewSize = ((FramebufferResizedEvent&)event).GetSize();
        }
    }

    void VulkanSwapchain::RT_Invalidate() {
        ZoneScoped;
        auto device = m_Device->GetDevice();

        // todo: delete framebuffers

        auto old = RT_Create();
        if (old != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, old, &VulkanContext::GetAllocCallbacks());
        }

        if (m_Swapchain != VK_NULL_HANDLE) {
            uint32_t imageCount;
            vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);

            std::vector<VkImage> images(imageCount);
            vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, images.data());

            for (uint32_t i = 0; i < imageCount; i++) {
                VulkanImage::VulkanSpec spec;
                spec.ExistingImage = images[i];
                spec.Extent.width = m_Extent.width;
                spec.Extent.height = m_Extent.height;
                spec.Format = m_ImageFormat;
                spec.ViewType = VK_IMAGE_VIEW_TYPE_2D;
                spec.AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
                spec.ArrayLayers = 1;
                spec.MipLevels = 1;

                auto swapchainImage = Ref<VulkanImage>::Create(m_Device, spec);

                // todo: framebuffer...
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

        if (vkCreateSwapchainKHR(m_Device->GetDevice(), &createInfo,
                                 &VulkanContext::GetAllocCallbacks(), &m_Swapchain) != VK_SUCCESS) {
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

        return VK_PRESENT_MODE_FIFO_KHR;
    }
} // namespace fuujin