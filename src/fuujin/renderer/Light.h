#pragma once
#include "fuujin/core/Ref.h"

namespace fuujin {
    class ShaderBuffer;

    class Light : public RefCounted {
    public:
        enum class Color { Diffuse, Ambient, Specular };

        enum class Type : int32_t {
            Point = 0,
        };

        struct Attenuation {
            float InfluenceRadius;
            float Falloff;
        };

        static const std::string& GetColorName(Color color);

        void SetUniforms(ShaderBuffer& data, const glm::mat4& transform = glm::mat4(1.f)) const;

        virtual Type GetType() const = 0;

        virtual const Attenuation& GetAttenuation() const = 0;
        virtual void SetAttenuation(const Attenuation& attenuation) = 0;

        virtual const std::unordered_map<Color, glm::vec3>& GetColors() const = 0;
        virtual void SetColor(Color name, const glm::vec3& color) = 0;

    protected:
        virtual void SetLightData(ShaderBuffer& data, const glm::mat4& transform) const = 0;
    };

    class PointLight : public Light {
    public:
        PointLight(const glm::vec3& position = glm::vec3(0.f),
                   const std::optional<Attenuation>& attenuation = {});

        virtual ~PointLight() override = default;

        virtual Type GetType() const override { return Type::Point; }

        const glm::vec3& GetPosition() const { return m_Position; }
        const glm::vec2& GetShadowZRange() const { return m_ShadowZRange; }

        virtual const Attenuation& GetAttenuation() const override { return m_Attenuation; }

        virtual const std::unordered_map<Color, glm::vec3>& GetColors() const override {
            return m_Colors;
        }

        void SetPosition(const glm::vec3& position);
        void SetShadowZRange(const glm::vec2& zRange);

        virtual void SetAttenuation(const Attenuation& attenuation) override;
        virtual void SetColor(Color name, const glm::vec3& color) override;

    protected:
        virtual void SetLightData(ShaderBuffer& data, const glm::mat4& transform) const override;

    private:
        glm::vec3 m_Position;
        glm::vec2 m_ShadowZRange;

        Attenuation m_Attenuation;
        std::unordered_map<Color, glm::vec3> m_Colors;
    };
} // namespace fuujin
