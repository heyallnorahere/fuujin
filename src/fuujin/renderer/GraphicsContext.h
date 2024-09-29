#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    class GraphicsContext : public RefCounted {
    public:
        static Ref<GraphicsContext> Get(const std::optional<std::string>& deviceName = {});
    };
} // namespace fuujin