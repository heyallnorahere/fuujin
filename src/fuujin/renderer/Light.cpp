#include "fuujinpch.h"
#include "fuujin/renderer/Light.h"

#include "fuujin/renderer/ShaderBuffer.h"

namespace fuujin {
    static const std::unordered_map<Light::Color, std::string> s_ColorNameMap = {
        { Light::Color::Diffuse, "Diffuse" },
        { Light::Color::Specular, "Specular" },
        { Light::Color::Ambient, "Ambient" }
    };

    const std::string& Light::GetColorName(Color color) {
        ZoneScoped;

        if (s_ColorNameMap.contains(color)) {
            return s_ColorNameMap.at(color);
        }

        throw std::runtime_error("Invalid light color name!");
    }

    void Light::SetUniforms(ShaderBuffer& data, const glm::mat4& transform) const {
        ZoneScoped;

        data.Set("Type", GetType());
        SetLightData(data, transform);

        ShaderBuffer attenuationSlice;
        if (data.Slice("Attenuation", attenuationSlice)) {
            const auto& attenuation = GetAttenuation();

            attenuationSlice.Set("InfluenceRadius", attenuation.InfluenceRadius);
            attenuationSlice.Set("Falloff", attenuation.Falloff);
        }

        ShaderBuffer colorSlice;
        if (data.Slice("Colors", colorSlice)) {
            const auto& colors = GetColors();

            for (const auto& [color, name] : s_ColorNameMap) {
                glm::vec3 value(1.f);
                if (colors.contains(color)) {
                    value = colors.at(color);
                }

                colorSlice.Set(name, value);
            }
        }
    }

    PointLight::PointLight(const glm::vec3& position,
                           const std::optional<Attenuation>& attenuation) {
        ZoneScoped;

        m_Position = position;
        m_ShadowZRange = glm::vec2(0.1f, 100.f);

        if (attenuation.has_value()) {
            m_Attenuation = attenuation.value();
        } else {
            // todo: calculate attenuation based on a default distance

            m_Attenuation.InfluenceRadius = 10.f;
            m_Attenuation.Falloff = 8.f;
        }
    }

    void PointLight::SetPosition(const glm::vec3& position) {
        ZoneScoped;

        m_Position = position;
    }

    void PointLight::SetShadowZRange(const glm::vec2& zRange) {
        ZoneScoped;

        m_ShadowZRange = zRange;
    }

    void PointLight::SetAttenuation(const Attenuation& attenuation) {
        ZoneScoped;

        m_Attenuation = attenuation;
    }

    void PointLight::SetColor(Color name, const glm::vec3& color) {
        ZoneScoped;

        m_Colors[name] = color;
    }

    void PointLight::SetLightData(ShaderBuffer& data, const glm::mat4& transform) const {
        ZoneScoped;

        glm::vec4 position = transform * glm::vec4(m_Position, 1.f);
        data.Set("Position", glm::vec3(position));
        data.Set("ShadowZRange", m_ShadowZRange);
    }
} // namespace fuujin
