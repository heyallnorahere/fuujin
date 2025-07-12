#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    enum class Cursor {
        Disabled,
        Arrow,
        TextInput,
        Hand,
        NotAllowed,
        ResizeNS,
        ResizeEW,
        ResizeNESW,
        ResizeNWSE,
        ResizeAll
    };

    struct ViewSize {
        uint32_t Width, Height;
    };

    class View : public RefCounted {
    public:
        virtual void Update() = 0;
        virtual bool IsClosed() const = 0;

        virtual uint64_t GetID() const = 0;

        virtual ViewSize GetSize() const = 0;
        virtual ViewSize GetFramebufferSize() const = 0;
        virtual void SetSize(const ViewSize& size) = 0;

        virtual void GetPosition(uint32_t& x, uint32_t& y) const = 0;
        virtual void SetPosition(uint32_t x, uint32_t y) = 0;

        virtual void SetCursor(Cursor cursor) = 0;

        virtual void GetCursorPosition(double& x, double& y) const = 0;
        virtual void SetCursorPosition(double x, double y) = 0;

        virtual void GetRequiredVulkanExtensions(std::vector<std::string>& extensions) const = 0;
        virtual void* CreateVulkanSurface(void* instance) const = 0;
    };
} // namespace fuujin