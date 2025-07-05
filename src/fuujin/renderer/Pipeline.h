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
            Type PipelineType;
            std::map<uint32_t, uint32_t> AttributeBindings;

            DepthSpec Depth;
            FrontFace PolygonFrontFace = FrontFace::CCW;
            bool DisableCulling = false;
            bool Wireframe = false;
            ColorBlending Blending = ColorBlending::Default;

            Ref<Shader> PipelineShader;
            Ref<RenderTarget> Target;
        };

        virtual const Spec& GetSpec() const = 0;
    };
} // namespace fuujin