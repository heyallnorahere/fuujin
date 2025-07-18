#include "fuujinpch.h"
#include "fuujin/scene/Camera.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    template <typename _Ty>
    inline glm::mat<4, 4, _Ty, glm::defaultp> Perspective(const GraphicsDevice::APISpec& api,
                                                          _Ty fovy, _Ty aspect, _Ty zNear,
                                                          _Ty zFar) {
        ZoneScoped;

        switch (api.Depth) {
        case GraphicsDevice::DepthRange::NegativeOneToOne:
            if (api.LeftHanded) {
                return glm::perspectiveLH_NO(fovy, aspect, zNear, zFar);
            } else {
                return glm::perspectiveRH_NO(fovy, aspect, zNear, zFar);
            }
        case GraphicsDevice::DepthRange::ZeroToOne:
            if (api.LeftHanded) {
                return glm::perspectiveLH_ZO(fovy, aspect, zNear, zFar);
            } else {
                return glm::perspectiveRH_ZO(fovy, aspect, zNear, zFar);
            }
        }

        return glm::mat<4, 4, _Ty, glm::defaultp>((_Ty)0);
    }

    template <typename _Ty>
    inline glm::mat<4, 4, _Ty, glm::defaultp> Orthographic(const GraphicsDevice::APISpec& api,
                                                           _Ty left, _Ty right, _Ty bottom, _Ty top,
                                                           _Ty zNear, _Ty zFar) {
        ZoneScoped;

        switch (api.Depth) {
        case GraphicsDevice::DepthRange::NegativeOneToOne:
            if (api.LeftHanded) {
                return glm::orthoLH_NO(left, right, bottom, top, zNear, zFar);
            } else {
                return glm::orthoRH_NO(left, right, bottom, top, zNear, zFar);
            }
        case GraphicsDevice::DepthRange::ZeroToOne:
            if (api.LeftHanded) {
                return glm::orthoLH_ZO(left, right, bottom, top, zNear, zFar);
            } else {
                return glm::orthoRH_ZO(left, right, bottom, top, zNear, zFar);
            }
        }

        return glm::mat<4, 4, _Ty, glm::defaultp>((_Ty)0);
    }

    template <typename _Ty, glm::qualifier Q>
    inline glm::mat<4, 4, _Ty, Q> LookAt(const GraphicsDevice::APISpec& api,
                                         const glm::vec<3, _Ty, Q>& eye,
                                         const glm::vec<3, _Ty, Q>& center,
                                         const glm::vec<3, _Ty, Q>& up) {
        ZoneScoped;

        if (api.LeftHanded) {
            return glm::lookAtLH(eye, center, up);
        } else {
            return glm::lookAtRH(eye, center, up);
        }
    }

    Camera::Camera() {
        ZoneScoped;

        SetType(Type::Perspective);
        SetViewSize(ViewSize(1, 1));
        SetVerticalFOV(glm::radians(45.f));
    }

    glm::mat4 Camera::CalculateViewProjection(const glm::vec3& translation,
                                              const glm::quat& rotation) {
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

        glm::mat3 rotationMatrix = glm::toMat3(rotation);
        glm::vec3 direction = glm::normalize(rotationMatrix * defaultDirection);
        glm::vec3 up = glm::normalize(rotationMatrix * defaultUp);

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
