#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/Shader.h"
#include "fuujin/renderer/Framebuffer.h"

namespace fuujin {
    enum class FrontFace {
        CCW,
        CW,
    };

    enum class ColorBlending {
        None,
        Default,
        Additive,
        OneZero,
        ZeroSourceColor
    };

    struct DepthSpec {
        bool Test = true;
        bool Write = true;
    };

    class Pipeline : public RefCounted {
    public:
        enum class Type {
            Graphics,
            Compute
        };

        struct Spec {
            Type Type;
            DepthSpec Depth;
            FrontFace FrontFace = FrontFace::CCW;
            bool DisableCulling = false;
            bool Wireframe = false;
            uint32_t MultisampleBits = 1;
            ColorBlending ColorBlending = ColorBlending::Default;

            Ref<Shader> Shader;
            Ref<RenderTarget> Target;
        };

        virtual const Spec& GetSpec() const = 0;
    };
} // namespace fuujin