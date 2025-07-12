#pragma once
#include "fuujin/core/View.h"

namespace fuujin {
    class PlatformAPI {
    public:
        virtual ~PlatformAPI() = default;

        virtual bool HasCursors() const = 0;
        virtual bool CanSetCursorPos() const = 0;

        virtual Ref<View> CreateView(const std::string& title, const ViewSize& size) const = 0;
    };

    class Platform {
    public:
        Platform() = delete;

        static void Init();
        static void Shutdown();

        static Ref<View> CreateView(const std::string& title, const ViewSize& size);
    };
} // namespace fuujin