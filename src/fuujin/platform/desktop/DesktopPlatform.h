#pragma once
#include "fuujin/core/Platform.h"

struct GLFWcursor;

namespace fuujin {
    class DesktopPlatform : public PlatformAPI {
    public:
        DesktopPlatform();
        virtual ~DesktopPlatform() override;

        virtual bool HasCursors() const override { return true; }
        virtual bool CanSetCursorPos() const override { return true; }

        virtual Ref<View> CreateView(const std::string& title, const ViewSize& size) const override;

        GLFWcursor* GetCursor(Cursor name) const;
    
    private:
        std::map<Cursor, GLFWcursor*> m_Cursors;
    };
} // namespace fuujin