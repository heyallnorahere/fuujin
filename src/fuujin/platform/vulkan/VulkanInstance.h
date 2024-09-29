#pragma once
#include "fuujin/core/Ref.h"
#include "fuujin/platform/vulkan/Vulkan.h"

namespace fuujin {
    class VulkanInstance : public RefCounted {
    public:
        struct Spec {
            uint32_t API;

            std::string ApplicationName;
            uint32_t ApplicationVersion;

            std::string EngineName;
            uint32_t EngineVersion;

            std::unordered_set<std::string> Extensions, Layers;
        };

        static void GetExtensions(std::unordered_set<std::string>& extensions);
        static void GetLayers(std::unordered_set<std::string>& layers);

        VulkanInstance(const Spec& spec);
        virtual ~VulkanInstance() override;

        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;

        const Spec& GetSpec() const { return m_Spec; }
        VkInstance GetInstance() const { return m_Instance; }

        void GetDevices(std::vector<VkPhysicalDevice>& devices) const;

    private:
        Spec m_Spec;
        VkInstance m_Instance;
    };
} // namespace fuujin