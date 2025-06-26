#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    struct ImageCopy {
        uint32_t X, Y, Z;
        uint32_t Width, Height, Depth;

        uint32_t MipLevel;
        uint32_t ArrayOffset, ArraySize;
    };

    class DeviceImage : public RefCounted {
    public:
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual uint32_t GetDepth() const = 0;

        virtual uint32_t GetMipLevels() const = 0;

        // todo: more API agnostic functionality
        // cant think of any tho
    };
} // namespace fuujin