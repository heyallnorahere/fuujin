#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    enum class QueueType { Graphics, Transfer, Compute };
    enum class GraphicsDeviceType { Discrete, CPU, Integrated, Other };

    class GraphicsDevice : public RefCounted {
    public:
        enum class DepthRange {
            ZeroToOne,
            NegativeOneToOne,
        };

        struct APISpec {
            std::string Name;
            Version APIVersion;

            bool TransposeMatrices;
            bool LeftHanded;
            DepthRange Depth;
        };

        struct Properties {
            std::string Name, Vendor;
            Version DriverVersion;
            GraphicsDeviceType Type;

            APISpec API;
        };

        virtual ~GraphicsDevice() = default;

        virtual void GetProperties(Properties& props) const = 0;

        virtual void Wait() const = 0;
    };
} // namespace fuujin
