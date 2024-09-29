#include "fuujinpch.h"
#include "fuujin/core/View.h"

#ifdef FUUJIN_PLATFORM_desktop
#include "fuujin/platform/desktop/DesktopWindow.h"
#endif

namespace fuujin {
    Ref<View> View::Create(const std::string& title, const ViewSize& size) {
#ifdef FUUJIN_PLATFORM_desktop
        return Ref<DesktopWindow>::Create(title, size);
#else
        FUUJIN_ERROR("No window platforms are supported!");
        return nullptr;
#endif
    }
} // namespace fuujin