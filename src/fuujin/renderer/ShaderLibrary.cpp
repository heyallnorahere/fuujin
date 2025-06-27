#include "fuujinpch.h"
#include "fuujin/renderer/ShaderLibrary.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    const std::unordered_map<std::string, std::vector<uint8_t>>& GetEmbeddedShaders();

    static const std::unordered_map<std::string, ShaderStage> s_StageMap = {
        { "Vertex", ShaderStage::Vertex },
        { "Fragment", ShaderStage::Fragment },
        { "Geometry", ShaderStage::Geometry },
        { "Compute", ShaderStage::Compute }
    };

    ShaderLibrary::ShaderLibrary(Ref<GraphicsContext> context) {
        ZoneScoped;
        m_Context = context;

        LoadEmbedded();
    }

    ShaderLibrary::~ShaderLibrary() {
        ZoneScoped;

        // ?
    }

    void ShaderLibrary::Load(const std::string& identifier, const ShaderSpec& spec) {
        ZoneScoped;
        if (m_Shaders.contains(identifier)) {
            throw std::runtime_error("Shader \"" + identifier + "\" already loaded!");
        }

        auto& data = m_Shaders[identifier];
        data.Spec = spec;

        FUUJIN_INFO("Loading shader {}", identifier.c_str());
        ReloadShader(identifier);
    }

    void ShaderLibrary::ReloadShader(const std::string& identifier) {
        ZoneScoped;
        if (!m_Shaders.contains(identifier)) {
            throw std::runtime_error("No such shader: " + identifier);
        }

        auto& data = m_Shaders[identifier];
        if (data.Shader) {
            Renderer::FreeShader(data.Shader->GetID());
        }

        if (data.Spec.Load.has_value()) {
            Shader::Code code;
            data.Spec.Load.value()(code);
            data.Shader = m_Context->LoadShader(code);
        } else {
            data.Shader = m_Context->LoadShader(data.Spec.StaticCode);
        }

        if (data.Shader.IsEmpty()) {
            throw std::runtime_error("Failed to load shader!");
        }
    }

    void ShaderLibrary::ReloadAll() {
        ZoneScoped;

        for (const auto& [identifier, data] : m_Shaders) {
            ReloadShader(identifier);
        }
    }

    Ref<Shader> ShaderLibrary::Get(const std::string& identifier) const {
        ZoneScoped;

        Ref<Shader> shader;
        if (m_Shaders.contains(identifier)) {
            shader = m_Shaders.at(identifier).Shader;
        }

        return shader;
    }

    void ShaderLibrary::LoadEmbedded() {
        ZoneScoped;

        std::unordered_map<std::string, Shader::Code> code;
        for (const auto& [path, spv] : GetEmbeddedShaders()) {
            std::string shaderName, spvName, stageName;

            size_t separator = path.find_last_of('/');
            if (separator != std::string::npos) {
                shaderName = path.substr(0, separator);
                spvName = path.substr(separator + 1);
            } else {
                spvName = path;
            }

            separator = spvName.find('.');
            if (separator != std::string::npos) {
                stageName = spvName.substr(0, separator);
            } else {
                stageName = spvName;
            }

            if (!s_StageMap.contains(stageName)) {
                throw std::runtime_error("Invalid stage in embedded shader!");
            }

            auto stage = s_StageMap.at(stageName);
            code[shaderName][stage] = spv;
        }

        for (const auto& [identifier, shaderCode] : code) {
            auto id = identifier;
            if (!id.empty()) {
                id = '/' + id;
            }

            ShaderSpec spec;
            spec.StaticCode = shaderCode;

            Load("assets/shaders" + id, spec);
        }
    }
}