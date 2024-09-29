#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

#include "fuujin/core/Application.h"
#include "fuujin/core/View.h"
#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    static void* VKAPI_CALL VulkanAlloc(void* pUserData, size_t size, size_t alignment,
                                        VkSystemAllocationScope allocationScope) {
        void* block = std::malloc(size);
        TracyAlloc(block, size);
        return block;
    }

    static void* VKAPI_CALL VulkanRealloc(void* pUserData, void* pOriginal, size_t size,
                                          size_t alignment,
                                          VkSystemAllocationScope allocationScope) {
        TracyFree(pOriginal);

        void* newBlock = std::realloc(pOriginal, size);
        TracyAlloc(newBlock, size);

        return newBlock;
    }

    static void VKAPI_CALL VulkanFree(void* pUserData, void* pMemory) {
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
        std::unordered_map<std::string, Ref<VulkanDevice>> Devices;
        std::string UsedDevice;
    };

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

    static uint64_t ScoreDevice(Ref<VulkanDevice> device) {
        ZoneScoped;

        VkPhysicalDeviceProperties2 properties{};
        device->GetProperties(properties);

        VkPhysicalDeviceFeatures2 features{};
        device->GetFeatures(features);

        VkPhysicalDeviceMemoryProperties memoryProperties{};
        device->GetMemoryProperties(memoryProperties);

        uint64_t score = 0;
        if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000000;
        }

        for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; i++) {
            const auto& heap = memoryProperties.memoryHeaps[i];
            if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                score += heap.size * 10;
            }
        }

        return score;
    }

    static void SelectDevice(ContextData* data) {
        ZoneScoped;

        std::string highestScoreDevice;
        uint64_t highestScore = 0;

        for (auto [name, device] : data->Devices) {
            uint64_t score = ScoreDevice(device);
            if (score > highestScore) {
                highestScore = score;
                highestScoreDevice = name;
            }
        }

        data->UsedDevice = highestScoreDevice;
    }

    VulkanContext::VulkanContext(const std::optional<std::string>& deviceName) {
        ZoneScoped;
        volkInitialize();
        FUUJIN_DEBUG("Global functions loaded");

        s_CurrentContext = this;
        m_Data = new ContextData;

        VulkanInstance::Spec spec;
        spec.API = VK_API_VERSION_1_2;
        spec.ApplicationName = "???";
        spec.ApplicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);
        spec.EngineName = "fuujin";
        spec.EngineVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);

        View* view = nullptr;
        try {
            view = &Application::Get().GetView();
        } catch (const std::runtime_error& exc) {
            view = nullptr;
        }

        if (view != nullptr) {
            std::vector<std::string> viewExtensions;
            view->GetRequiredVulkanExtensions(viewExtensions);
            spec.Extensions.insert(viewExtensions.begin(), viewExtensions.end());
        }

        m_Data->Instance = Ref<VulkanInstance>::Create(spec);
        Renderer::Wait();

        volkLoadInstanceOnly(m_Data->Instance->GetInstance());
        FUUJIN_DEBUG("Instance functions loaded");

        std::vector<VkPhysicalDevice> devices;
        m_Data->Instance->GetDevices(devices);
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
            SelectDevice(m_Data);
            FUUJIN_INFO("Selected Vulkan device: {}", m_Data->UsedDevice.c_str());
        }
    }

    VulkanContext::~VulkanContext() {
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

} // namespace fuujin