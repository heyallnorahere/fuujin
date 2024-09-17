#include "fuujinpch.h"
#include "fuujin/View.h"

#ifdef FUUJIN_PLATFORM_desktop
#include "fuujin/platform/desktop/DesktopWindow.h"
#endif

namespace fuujin {
    View* View::Create(const std::string& title, const ViewSize& size) {
#ifdef FUUJIN_PLATFORM_desktop
        return new DesktopWindow(title, size);
#else
        return nullptr;
#endif
    }
} // namespace fuujin