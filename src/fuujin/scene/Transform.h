#pragma once

namespace fuujin {
    class Transform {
    public:
        Transform();
        ~Transform() = default;

        Transform(const glm::vec3& translation, const glm::quat& rotationQuat,
                  const glm::vec3& scale);

        Transform(const glm::vec3& translation, const glm::vec3& rotationEuler,
                  const glm::vec3& scale);

        Transform(const glm::mat4& matrix);

        Transform(const Transform& other);
        Transform& operator=(const Transform& other);

        const glm::vec3& GetTranslation() const { return m_Translation; }
        const glm::vec3& GetRotationEuler() const { return m_RotationEuler; }
        const glm::quat& GetRotationQuat() const { return m_RotationQuat; }
        const glm::vec3& GetScale() const { return m_Scale; }

        void SetTranslation(const glm::vec3& translation) {
            ZoneScoped;

            m_Translation = translation;
        }

        void SetRotation(const glm::vec3& rotationEuler) {
            ZoneScoped;

            m_RotationEuler = rotationEuler;
            m_RotationQuat = glm::quat(rotationEuler);
        }

        void SetRotation(const glm::quat& rotationQuat) {
            ZoneScoped;

            m_RotationQuat = rotationQuat;
            m_RotationEuler = glm::eulerAngles(rotationQuat);
        }

        void SetScale(const glm::vec3& scale) {
            ZoneScoped;

            m_Scale = scale;
        }

        glm::mat4 ToMatrix() const;

    private:
        glm::vec3 m_Translation;
        glm::vec3 m_RotationEuler;
        glm::quat m_RotationQuat;
        glm::vec3 m_Scale;
    };
} // namespace fuujin
