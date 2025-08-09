#pragma once

#include "fuujin/core/View.h"

#include "fuujin/renderer/GraphicsDevice.h"

namespace fuujin {
    class Camera {
    public:
        template <typename _Ty>
        inline static glm::mat<4, 4, _Ty, glm::defaultp> Perspective(
            const GraphicsDevice::APISpec& api, _Ty fovy, _Ty aspect, _Ty zNear, _Ty zFar) {
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
        inline static glm::mat<4, 4, _Ty, glm::defaultp> Orthographic(
            const GraphicsDevice::APISpec& api, _Ty left, _Ty right, _Ty bottom, _Ty top, _Ty zNear,
            _Ty zFar) {
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
        inline static glm::mat<4, 4, _Ty, Q> LookAt(const GraphicsDevice::APISpec& api,
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

        enum class Type {
            Perspective,
            Orthographic,
        };

        Camera();
        ~Camera() = default;

        Camera(const Camera&) = default;
        Camera& operator=(const Camera&) = default;

        Type GetType() const { return m_Type; }
        const ViewSize& GetViewSize() const { return m_ViewSize; }

        float GetVerticalFOV() const { return m_VerticalFOV; }
        const glm::vec2& GetZRange() const { return m_ZRange; }

        void SetType(Type type) {
            ZoneScoped;

            m_OutOfDate |= type != m_Type;
            m_Type = type;
        }

        void SetViewSize(const ViewSize& size) {
            ZoneScoped;

            m_OutOfDate |= size.Width != m_ViewSize.Width || size.Height != m_ViewSize.Height;
            m_ViewSize = size;
        }

        void SetVerticalFOV(float fov) {
            ZoneScoped;

            m_OutOfDate |= m_Type == Type::Perspective;
            m_VerticalFOV = fov;
        }

        void SetZRange(const glm::vec2& range) {
            ZoneScoped;

            m_OutOfDate = true;
            m_ZRange = range;
        }

        glm::mat4 CalculateViewProjection(const glm::mat4& transform);

    private:
        void UpdateProjection(const GraphicsDevice::APISpec& api);

        Type m_Type;
        ViewSize m_ViewSize;

        float m_VerticalFOV;
        glm::vec2 m_ZRange;

        bool m_OutOfDate;
        glm::mat4 m_Projection;
    };
} // namespace fuujin
