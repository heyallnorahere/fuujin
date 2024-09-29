#include "fuujinpch.h"
#include "fuujin/platform/vulkan/VulkanContext.h"
#include "fuujin/platform/vulkan/VulkanInstance.h"

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

    VulkanContext::VulkanContext(const std::optional<std::string>& deviceName) {
        ZoneScoped;
        Renderer::Submit([]() { volkInitialize(); });

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
        Renderer::Submit([this]() { volkLoadInstance(m_Data->Instance->GetInstance()); });
    }

    VulkanContext::~VulkanContext() {
        delete m_Data;
        s_CurrentContext = nullptr;
    }
} // namespace fuujin