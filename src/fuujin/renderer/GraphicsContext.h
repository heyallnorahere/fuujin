#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/GraphicsDevice.h"

namespace fuujin {
    class GraphicsContext : public RefCounted {
    public:
        static Ref<GraphicsContext> Get(const std::optional<std::string>& deviceName = {});

        virtual Ref<GraphicsDevice> GetDevice() const = 0;
    };
} // namespace fuujin