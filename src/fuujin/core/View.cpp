#include "fuujinpch.h"
#include "fuujin/core/View.h"

#ifdef FUUJIN_PLATFORM_desktop
#include "fuujin/platform/desktop/DesktopWindow.h"
#endif

namespace fuujin {
    View* View::Create(const std::string& title, const ViewSize& size) {
#ifdef FUUJIN_PLATFORM_desktop
        return new DesktopWindow(title, size);
#else
        FUUJIN_ERROR("No window platforms are supported!");
        return nullptr;
#endif
    }
} // namespace fuujin