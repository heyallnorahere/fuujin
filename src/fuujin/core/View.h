#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    struct ViewSize {
        uint32_t Width, Height;
    };

    class View : public RefCounted {
    public:
        static Ref<View> Create(const std::string& title, const ViewSize& size);

        virtual ~View() = default;

        virtual void Update() = 0;
        virtual bool IsClosed() const = 0;

        virtual ViewSize GetSize() const = 0;
        virtual ViewSize GetFramebufferSize() const = 0;
        virtual void RequestSize(const ViewSize& size) = 0;

        virtual void GetRequiredVulkanExtensions(std::vector<std::string>& extensions) = 0;
        virtual void* CreateVulkanSurface(void* instance) = 0;
    };
} // namespace fuujin