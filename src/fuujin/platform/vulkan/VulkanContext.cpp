#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

#include "fuujin/core/Application.h"
#include "fuujin/renderer/Renderer.h"

#include "fuujin/platform/vulkan/VulkanCommandQueue.h"
#include "fuujin/platform/vulkan/VulkanShader.h"
#include "fuujin/platform/vulkan/VulkanPipeline.h"
#include "fuujin/platform/vulkan/VulkanRenderer.h"
#include "fuujin/platform/vulkan/VulkanBuffer.h"

namespace fuujin {
    static void* VKAPI_CALL VulkanAlloc(void* pUserData, size_t size, size_t alignment,
                                        VkSystemAllocationScope allocationScope) {
        ZoneScoped;

        void* block = std::malloc(size);
        TracyAlloc(block, size);
        return block;
    }

    static void* VKAPI_CALL VulkanRealloc(void* pUserData, void* pOriginal, size_t size,
                                          size_t alignment,
                                          VkSystemAllocationScope allocationScope) {
        ZoneScoped;
        TracyFree(pOriginal);

        void* newBlock = std::realloc(pOriginal, size);
        TracyAlloc(newBlock, size);

        return newBlock;
    }

    static void VKAPI_CALL VulkanFree(void* pUserData, void* pMemory) {
        ZoneScoped;

        TracyFree(pMemory);
        std::free(pMemory);
    }

    VkAllocationCallbacks& VulkanContext::GetAllocCallbacks() {
        ZoneScoped;

        static std::unique_ptr<VkAllocationCallbacks> callbacks;
        if (!callbacks) {
            callbacks = std::unique_ptr<VkAllocationCallbacks>(new VkAllocationCallbacks{});
            callbacks->pfnAllocation = VulkanAlloc;
            callbacks->pfnReallocation = VulkanRealloc;
            callbacks->pfnFree = VulkanFree;
        }

        return *callbacks;
    }

    struct ContextData {
        Ref<VulkanInstance> Instance;
        VkDebugUtilsMessengerEXT DebugMessenger = VK_NULL_HANDLE;

        std::unordered_map<std::string, Ref<VulkanDevice>> Devices;
        std::string UsedDevice;

        Ref<VulkanSwapchain> Swapchain;
        VmaAllocator Allocator = VK_NULL_HANDLE;

        std::unordered_map<QueueType, Ref<VulkanCommandQueue>> Queues;
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        ZoneScoped;

        std::string category;
        switch (messageType) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            category = "GENERAL";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            category = "VALIDATION";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            category = "PERFORMANCE";
            break;
        default:
            category = "UNKNOWN";
            break;
        }

        std::string message = "[VULKAN] [" + category + "] " + std::string(pCallbackData->pMessage);
        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            FUUJIN_DEBUG("{}", message.c_str());
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            FUUJIN_WARN("{}", message.c_str());
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            FUUJIN_ERROR("{}", message.c_str());
            break;
        default:
            FUUJIN_INFO("{}", message.c_str());
            break;
        }

        return VK_FALSE;
    }

    static VulkanContext* s_CurrentContext = nullptr;

    Ref<VulkanContext> VulkanContext::Get(const std::optional<std::string>& deviceName) {
        ZoneScoped;

        VulkanContext* context = nullptr;
        if (s_CurrentContext != nullptr) {
            context = s_CurrentContext;
        } else {
            context = new VulkanContext(deviceName);
        }

        return Ref<VulkanContext>(context);
    }

    static uint64_t RT_ScoreDevice(Ref<VulkanDevice> device) {
        ZoneScoped;

        VkPhysicalDeviceProperties2 properties{};
        device->RT_GetProperties(properties);

        VkPhysicalDeviceFeatures2 features{};
        device->RT_GetFeatures(features);

        VkPhysicalDeviceMemoryProperties memoryProperties{};
        device->RT_GetMemoryProperties(memoryProperties);

        uint64_t score = 0;
        for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; i++) {
            const auto& heap = memoryProperties.memoryHeaps[i];
            if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                score += heap.size;
            }
        }

        if (properties.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) {
            score *= 100; // we want to avoid cpu-based devices (i.e. llvmpipe)
        }

        if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score *= 10;
        }

        return score;
    }

    static void RT_SelectDevice(ContextData* data) {
        ZoneScoped;

        std::string highestScoreDevice;
        uint64_t highestScore = 0;

        for (auto [name, device] : data->Devices) {
            uint64_t score = RT_ScoreDevice(device);
            if (score > highestScore) {
                highestScore = score;
                highestScoreDevice = name;
            }
        }

        data->UsedDevice = highestScoreDevice;
    }

    VulkanContext::VulkanContext(const std::optional<std::string>& deviceName) {
        ZoneScoped;
        Renderer::Submit(
            []() {
                volkInitialize();
                FUUJIN_DEBUG("Global functions loaded");
            },
            "Load global functions");

        Ref<View> view;
        try {
            view = Application::Get().GetView();
        } catch (const std::runtime_error& exc) {
            view = nullptr;
        }

        s_CurrentContext = this;
        m_Data = new ContextData;

        {
            VulkanInstance::Spec spec;
            spec.API = VK_API_VERSION_1_2;
            spec.ApplicationName = "???";
            spec.ApplicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);
            spec.EngineName = "fuujin";
            spec.EngineVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);

            if (view.IsPresent()) {
                std::vector<std::string> viewExtensions;
                view->GetRequiredVulkanExtensions(viewExtensions);
                spec.Extensions.insert(viewExtensions.begin(), viewExtensions.end());
            }

#ifdef FUUJIN_IS_DEBUG
            spec.Extensions.insert("VK_EXT_debug_utils");
            spec.Layers.insert("VK_LAYER_KHRONOS_validation");
#endif

            m_Data->Instance = Ref<VulkanInstance>::Create(spec);
        }

        Renderer::Submit([&]() { RT_LoadInstance(); }, "Load instance functions");
        Renderer::Submit([&]() { RT_CreateDebugMessenger(); }, "Create debug messenger");
        Renderer::Submit([&]() { RT_EnumerateDevices(deviceName); }, "Enumerate devices");

        // one of the few times we sync - we want the device handles
        // specific calls like creating window surfaces are costly
        // so we want to minimize use of this function
        Renderer::Wait();
        auto device = m_Data->Devices[m_Data->UsedDevice];

        {
            auto& deviceSpec = device->GetSpec();
            deviceSpec.RequestedQueues = { QueueType::Graphics, QueueType::Transfer };
        }

        if (view.IsPresent()) {
            m_Data->Swapchain = Ref<VulkanSwapchain>::Create(view, device);
            Renderer::Submit([&]() { RT_QueryPresentQueue(); }, "Query present queue");
        }

        device->Initialize();
        Renderer::Submit([&]() { RT_LoadDevice(); }, "Load device functions");
        Renderer::Submit([&]() { RT_CreateAllocator(); }, "Create allocator");

        Renderer::Submit(
            [&]() {
                auto queueDevice = m_Data->Devices[m_Data->UsedDevice];
                const auto& queues = queueDevice->GetQueues();
                for (const auto& [type, family] : queues) {
                    auto commandQueue = Ref<VulkanCommandQueue>::Create(queueDevice, type);
                    m_Data->Queues.insert(std::make_pair(type, commandQueue));
                }
            },
            "Create command queues");

        if (m_Data->Swapchain.IsPresent()) {
            // passes by reference to the allocator pointer
            // so the value only matters when the render thread catches up
            // incidentally, this can only happen after the allocator has been created
            // so it's not a race condition, but it is kind of hacky
            m_Data->Swapchain->Initialize(m_Data->Allocator);
        }
    }

    VulkanContext::~VulkanContext() {
        ZoneScoped;

        for (auto [type, queue] : m_Data->Queues) {
            queue->Clear();
        }

        m_Data->Swapchain.Reset();
        m_Data->Queues.clear();

        auto allocator = m_Data->Allocator;
        Renderer::Submit([allocator]() mutable { vmaDestroyAllocator(allocator); },
                         "Destroy allocator");

        m_Data->Devices.clear();

        auto instance = m_Data->Instance->GetInstance();
        auto debugMessenger = m_Data->DebugMessenger;
        Renderer::Submit(
            [instance, debugMessenger]() mutable {
                if (debugMessenger != VK_NULL_HANDLE) {
                    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, &GetAllocCallbacks());
                }
            },
            "Destroy debug messenger");

        m_Data->Instance.Reset();

        delete m_Data;
        s_CurrentContext = nullptr;
    }

    Ref<VulkanInstance> VulkanContext::GetInstance() const { return m_Data->Instance; }

    Ref<VulkanDevice> VulkanContext::GetVulkanDevice(
        const std::optional<std::string>& deviceName) const {
        auto name = deviceName.value_or(m_Data->UsedDevice);
        return m_Data->Devices[name];
    }

    Ref<GraphicsDevice> VulkanContext::GetDevice() const {
        return m_Data->Devices[m_Data->UsedDevice];
    };

    Ref<Swapchain> VulkanContext::GetSwapchain() const { return m_Data->Swapchain; }

    Ref<CommandQueue> VulkanContext::GetQueue(QueueType type) const {
        Ref<CommandQueue> queue;
        if (m_Data->Queues.contains(type)) {
            queue = m_Data->Queues.at(type);
        }

        return queue;
    }

    Ref<Fence> VulkanContext::CreateFence(bool signaled) const {
        return Ref<VulkanFence>::Create(m_Data->Devices[m_Data->UsedDevice], signaled);
    }

    Ref<RefCounted> VulkanContext::CreateSemaphore() const {
        return Ref<VulkanSemaphore>::Create(m_Data->Devices[m_Data->UsedDevice]);
    }

    Ref<Shader> VulkanContext::LoadShader(const Shader::Code& code) const {
        return Ref<VulkanShader>::Create(m_Data->Devices[m_Data->UsedDevice], code);
    }

    Ref<Pipeline> VulkanContext::CreatePipeline(const Pipeline::Spec& spec) const {
        return Ref<VulkanPipeline>::Create(m_Data->Devices[m_Data->UsedDevice], spec);
    }

    Ref<DeviceBuffer> VulkanContext::CreateBuffer(const DeviceBuffer::Spec& spec) const {
        return Ref<VulkanBuffer>::Create(m_Data->Devices[m_Data->UsedDevice], m_Data->Allocator,
                                         spec);
    }

    RendererAPI* VulkanContext::CreateRendererAPI() const {
        return new VulkanRenderer(m_Data->Devices[m_Data->UsedDevice]);
    }

    void VulkanContext::RT_LoadInstance() const {
        ZoneScoped;

        volkLoadInstanceOnly(m_Data->Instance->GetInstance());
        FUUJIN_DEBUG("Instance functions loaded");
    }

    void VulkanContext::RT_LoadDevice() const {
        ZoneScoped;

        volkLoadDevice(m_Data->Devices[m_Data->UsedDevice]->GetDevice());
        FUUJIN_DEBUG("Device functions loaded");
    }

    void VulkanContext::RT_QueryPresentQueue() {
        ZoneScoped;

        auto queue = m_Data->Swapchain->RT_FindDeviceQueue();
        if (queue.has_value()) {
            auto device = m_Data->Devices[m_Data->UsedDevice];
            auto& spec = device->GetSpec();

            uint32_t renderQueue = queue.value();
            FUUJIN_DEBUG("Swapchain render queue found: {}", renderQueue);

            spec.AdditionalQueues.insert(renderQueue);
            spec.Extensions.insert("VK_KHR_swapchain");
        }
    }

    void VulkanContext::RT_CreateAllocator() {
        ZoneScoped;
        auto device = m_Data->Devices[m_Data->UsedDevice];

        VmaVulkanFunctions functions{};
        functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = m_Data->Instance->GetSpec().API;
        allocatorInfo.instance = m_Data->Instance->GetInstance();
        allocatorInfo.physicalDevice = device->GetPhysicalDevice();
        allocatorInfo.device = device->GetDevice();
        allocatorInfo.pAllocationCallbacks = &GetAllocCallbacks();
        allocatorInfo.pVulkanFunctions = &functions;

        if (vmaCreateAllocator(&allocatorInfo, &m_Data->Allocator) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create allocator!");
        }
    }

    void VulkanContext::RT_CreateDebugMessenger() {
        ZoneScoped;

        auto create = vkCreateDebugUtilsMessengerEXT;
        if (create == nullptr) {
            return;
        }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.pfnUserCallback = VulkanDebugCallback;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        if (create(m_Data->Instance->GetInstance(), &createInfo, &GetAllocCallbacks(),
                   &m_Data->DebugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create debug messenger!");
        }
    }

    void VulkanContext::RT_EnumerateDevices(const std::optional<std::string>& deviceName) {
        ZoneScoped;

        std::vector<VkPhysicalDevice> devices;
        m_Data->Instance->RT_GetDevices(devices);
        FUUJIN_DEBUG("{} devices found", devices.size());

        for (size_t i = 0; i < devices.size(); i++) {
            auto device = devices[i];
            auto vulkanDevice = Ref<VulkanDevice>::Create(m_Data->Instance, device);

            VulkanDevice::Properties props;
            vulkanDevice->GetProperties(props);

            m_Data->Devices.insert(std::make_pair(props.Name, vulkanDevice));
            FUUJIN_DEBUG("Device {}: {}", i, props.Name.c_str());
        }

        bool usingRequested = false;
        if (deviceName.has_value()) {
            std::string requestedDevice = deviceName.value();
            if (m_Data->Devices.contains(requestedDevice)) {
                usingRequested = true;
                m_Data->UsedDevice = requestedDevice;

                FUUJIN_INFO("Using requested Vulkan device: {}", requestedDevice.c_str());
            } else {
                FUUJIN_WARN("No such Vulkan device: {}", requestedDevice.c_str());
            }
        }

        if (!usingRequested) {
            RT_SelectDevice(m_Data);
            FUUJIN_INFO("Selected Vulkan device: {}", m_Data->UsedDevice.c_str());
        }
    }
} // namespace fuujin