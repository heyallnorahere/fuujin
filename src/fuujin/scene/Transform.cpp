#include "fuujinpch.h"
#include "fuujin/scene/Transform.h"

namespace fuujin {
    Transform::Transform() {
        ZoneScoped;

        SetTranslation(glm::vec3(0.f));
        SetRotation(glm::vec3(0.f));
        SetScale(glm::vec3(1.f));
    }

    Transform::Transform(const glm::vec3& translation, const glm::quat& rotationQuat,
                         const glm::vec3& scale) {
        ZoneScoped;

        SetTranslation(translation);
        SetRotation(rotationQuat);
        SetScale(scale);
    }

    Transform::Transform(const glm::vec3& translation, const glm::vec3& rotationEuler,
                         const glm::vec3& scale) {
        ZoneScoped;

        SetTranslation(translation);
        SetRotation(rotationEuler);
        SetScale(scale);
    }

    Transform::Transform(const glm::mat4& matrix) {
        ZoneScoped;

        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotation;

        if (!glm::decompose(matrix, scale, rotation, translation, skew, perspective)) {
            FUUJIN_WARN("Failed to decompose transform matrix! Setting to default values");

            SetTranslation(glm::vec3(0.f));
            SetRotation(glm::vec3(0.f));
            SetScale(glm::vec3(1.f));
        } else {
            SetTranslation(translation);
            SetRotation(rotation);
            SetScale(scale);
        }
    }

    glm::mat4 Transform::ToMatrix() const {
        ZoneScoped;

        glm::mat4 translation = glm::translate(glm::mat4(1.f), m_Translation);
        glm::mat4 rotation = glm::toMat4(m_RotationQuat);
        glm::mat4 scale = glm::scale(glm::mat4(1.f), m_Scale);

        return translation * rotation * scale;
    }
} // namespace fuujin
