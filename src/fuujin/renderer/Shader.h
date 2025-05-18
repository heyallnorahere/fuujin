#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    enum class ShaderStage { Vertex, Fragment, Geometry, Compute };

    class Shader : public RefCounted {
    public:
        using Code = std::unordered_map<ShaderStage, std::vector<uint8_t>>;

        virtual void GetStages(std::unordered_set<ShaderStage>& stages) const = 0;
    };
} // namespace fuujin