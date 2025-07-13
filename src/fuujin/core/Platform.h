#pragma once
#include "fuujin/core/View.h"

namespace fuujin {
    struct MonitorInfo {
        glm::ivec2 MainPos, WorkPos;
        glm::ivec2 MainSize, WorkSize;
        float Scale;
        void* Handle;
    };

    class PlatformAPI : public RefCounted {
    public:
        virtual bool HasCursors() const = 0;
        virtual bool CanSetCursorPos() const = 0;
        virtual bool HasViewports() const = 0;
        virtual bool HasViewHovered() const = 0;

        virtual std::string GetName() const = 0;

        virtual Ref<View> CreateView(const std::string& title, const ViewSize& size,
                                     const ViewCreationOptions& options) const = 0;
        
        virtual bool QueryMonitors(std::vector<MonitorInfo>& monitors) const = 0;
    };

    class Platform {
    public:
        Platform() = delete;

        static void Init();
        static void Shutdown();

        static bool HasCursors();
        static bool CanSetCursorPos();
        static bool HasViewports();
        static bool HasViewHovered();

        static std::string GetName();

        static Ref<View> CreateView(const std::string& title, const ViewSize& size,
                                    const ViewCreationOptions& options = ViewCreationOptions());

        static bool QueryMonitors(std::vector<MonitorInfo>& monitors);
    };
} // namespace fuujin