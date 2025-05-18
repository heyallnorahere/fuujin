#pragma once
#include "fuujin/renderer/GraphicsContext.h"
#include "fuujin/renderer/Shader.h"

namespace fuujin {
    using ShaderLoadCallback = std::function<void(Shader::Code& code)>;
    struct ShaderSpec {
        std::optional<ShaderLoadCallback> Load;
        Shader::Code StaticCode;
    };

    struct ShaderData {
        ShaderSpec Spec;
        Ref<Shader> Shader;
    };

    class ShaderLibrary {
    public:
        ShaderLibrary(Ref<GraphicsContext> context);
        ~ShaderLibrary();

        ShaderLibrary(const ShaderLibrary&) = delete;
        ShaderLibrary& operator=(const ShaderLibrary&) = delete;

        void Load(const std::string& identifier, const ShaderSpec& spec);

        void ReloadShader(const std::string& identifier);
        void ReloadAll();

        Ref<Shader> Get(const std::string& identifier) const;

    private:
        void LoadEmbedded();

        Ref<GraphicsContext> m_Context;
        std::unordered_map<std::string, ShaderData> m_Shaders;
    };
} // namespace fuujin