#include "fuujinpch.h"
#include "fuujin/renderer/Renderer.h"
#include "fuujin/platform/vulkan/VulkanInstance.h"
#include "fuujin/platform/vulkan/VulkanContext.h"

namespace fuujin {
    void VulkanInstance::RT_GetExtensions(std::unordered_set<std::string>& extensions) {
        ZoneScoped;

        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> data(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, data.data());

        extensions.clear();
        for (const auto& extension : data) {
            extensions.insert(extension.extensionName);
        }
    }

    void VulkanInstance::RT_GetLayers(std::unordered_set<std::string>& layers) {
        ZoneScoped;

        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> data(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, data.data());

        layers.clear();
        for (const auto& layer : data) {
            layers.insert(layer.layerName);
        }
    }

    VulkanInstance::VulkanInstance(const Spec& spec) : m_Spec(spec), m_Instance(nullptr) {
        ZoneScoped;
        Renderer::Submit([&]() { RT_Create(); }, "Create instance");
    }

    VulkanInstance::~VulkanInstance() {
        ZoneScoped;

        VkInstance instance = m_Instance;
        Renderer::Submit(
            [instance]() {
                ZoneScoped;
                vkDestroyInstance(instance, &VulkanContext::GetAllocCallbacks());
            },
            "Destroy instance");
    }

    void VulkanInstance::RT_GetDevices(std::vector<VkPhysicalDevice>& devices) const {
        ZoneScoped;

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        devices.resize(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());
    }

    void VulkanInstance::RT_Create() {
        ZoneScoped;

        std::unordered_set<std::string> availableExtensions, availableLayers;
        RT_GetExtensions(availableExtensions);
        RT_GetLayers(availableLayers);

        spdlog::enable_backtrace(availableExtensions.size());
        for (const auto& extension : availableExtensions) {
            FUUJIN_TRACE("Instance extension: {}", extension.c_str());
        }

        std::vector<const char*> extensions;
        for (const auto& extension : m_Spec.Extensions) {
            if (!availableExtensions.contains(extension)) {
                s_Logger.dump_backtrace();

                FUUJIN_ERROR("Extension {} is not supported!", extension.c_str());
            }

            extensions.push_back(extension.c_str());
        }

        spdlog::disable_backtrace();
        spdlog::enable_backtrace(availableLayers.size());

        for (const auto& layer : availableLayers) {
            FUUJIN_TRACE("Instance layer: {}", layer.c_str());
        }

        std::vector<const char*> layers;
        for (const auto& layer : m_Spec.Layers) {
            if (!availableLayers.contains(layer)) {
                s_Logger.dump_backtrace();

                FUUJIN_ERROR("Layer {} is not supported!", layer.c_str());
            }

            layers.push_back(layer.c_str());
        }

        spdlog::disable_backtrace();

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = m_Spec.API;
        appInfo.pApplicationName = m_Spec.ApplicationName.c_str();
        appInfo.applicationVersion = m_Spec.ApplicationVersion;
        appInfo.pEngineName = m_Spec.EngineName.c_str();
        appInfo.engineVersion = m_Spec.EngineVersion;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = (uint32_t)layers.size();
        createInfo.ppEnabledLayerNames = layers.data();

        if (vkCreateInstance(&createInfo, &VulkanContext::GetAllocCallbacks(), &m_Instance) !=
            VK_SUCCESS) {
            FUUJIN_CRITICAL("Failed to create instance!");

            return;
        }

        FUUJIN_DEBUG("Instance created with API version: {}.{}.{}", VK_VERSION_MAJOR(m_Spec.API),
                     VK_VERSION_MINOR(m_Spec.API), VK_VERSION_PATCH(m_Spec.API));
    }
} // namespace fuujin