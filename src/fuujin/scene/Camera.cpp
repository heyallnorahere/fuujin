#include "fuujinpch.h"
#include "fuujin/scene/Camera.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    Camera::Camera() {
        ZoneScoped;

        SetType(Type::Perspective);
        SetViewSize(ViewSize(1, 1));
        SetVerticalFOV(glm::radians(45.f));
        SetZRange(glm::vec2(0.1f, 100.f));
    }

    glm::mat4 Camera::CalculateViewProjection(const glm::mat4& transform) {
        ZoneScoped;

        auto& api = Renderer::GetAPI();
        if (m_OutOfDate) {
            UpdateProjection(api);
            m_OutOfDate = false;
        }

        // right hand rule
        // index finger to the right (+x)
        // middle finger up (+y)
        // thumb will be towards you (+z)
        // into the screen is -1
        static const glm::vec3 defaultDirection = glm::vec3(0.f, 0.f, -1.f);
        static const glm::vec3 defaultUp = glm::vec3(0.f, 1.f, 0.f);

        glm::mat3 rotationMatrix = glm::mat3(transform);
        glm::vec3 direction = glm::normalize(rotationMatrix * defaultDirection);
        glm::vec3 up = glm::normalize(rotationMatrix * defaultUp);

        glm::vec3 translation = glm::vec3(transform * glm::vec4(glm::vec3(0.f), 1.f));
        glm::mat4 view = LookAt(api, translation, translation + direction, up);

        return m_Projection * view;
    }

    void Camera::UpdateProjection(const GraphicsDevice::APISpec& api) {
        ZoneScoped;

        float zNear = m_ZRange[0];
        float zFar = m_ZRange[1];

        switch (m_Type) {
        case Type::Perspective: {
            float aspectRatio;
            if (m_ViewSize.Height > 0) {
                aspectRatio = (float)m_ViewSize.Width / m_ViewSize.Height;
            } else {
                aspectRatio = 1.f;
            }

            m_Projection = Perspective(api, m_VerticalFOV, aspectRatio, zNear, zFar);
        } break;
        case Type::Orthographic: {
            float halfWidth = (float)m_ViewSize.Width / 2.f;
            float halfHeight = (float)m_ViewSize.Height / 2.f;

            float left = -halfWidth;
            float right = halfWidth;

            float bottom = -halfHeight;
            float top = halfHeight;

            m_Projection = Orthographic(api, left, right, bottom, top, zNear, zFar);
        } break;
        }
    }
} // namespace fuujin
