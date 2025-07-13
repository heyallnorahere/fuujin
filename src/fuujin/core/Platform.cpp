#include "fuujinpch.h"
#include "fuujin/core/Platform.h"

#ifdef FUUJIN_PLATFORM_desktop
#include "fuujin/platform/desktop/DesktopPlatform.h"
#endif

namespace fuujin {
    static std::unique_ptr<PlatformAPI> s_API;

    static PlatformAPI* CreatePlatformAPI() {
        ZoneScoped;

#ifdef FUUJIN_PLATFORM_desktop
        return new DesktopPlatform;
#else
        return nullptr;
#endif
    }

    void Platform::Init() {
        ZoneScoped;

        s_API = std::unique_ptr<PlatformAPI>(CreatePlatformAPI());
        if (!s_API) {
            FUUJIN_ERROR("Failed to create platform API!");
        }
    }

    void Platform::Shutdown() {
        ZoneScoped;
        s_API.reset();
    }

    bool Platform::HasCursors() { return s_API->HasCursors(); }
    bool Platform::CanSetCursorPos() { return s_API->CanSetCursorPos(); }
    bool Platform::HasViewports() { return s_API->HasViewports(); }
    bool Platform::HasViewHovered() { return s_API->HasViewHovered(); }

    std::string Platform::GetName() { return s_API->GetName(); }

    Ref<View> Platform::CreateView(const std::string& title, const ViewSize& size,
                                   const ViewCreationOptions& options) {
        ZoneScoped;
        return s_API->CreateView(title, size, options);
    }

    bool Platform::QueryMonitors(std::vector<MonitorInfo>& monitors) {
        ZoneScoped;

        bool success = s_API->QueryMonitors(monitors);
        return success && !monitors.empty();
    }
} // namespace fuujin