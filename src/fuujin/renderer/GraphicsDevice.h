#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    class GraphicsDevice : public RefCounted {
    public:
        enum class DeviceType {
            Discrete,
            CPU,
            Integrated,
            Other
        };

        struct Properties {
            std::string Name, API;
            DeviceType Type;
        };

        virtual ~GraphicsDevice() = default;

        virtual void GetProperties(Properties& props) const = 0;
    };
}