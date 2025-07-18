#pragma once

#include "fuujin/core/View.h"

#include "fuujin/renderer/GraphicsDevice.h"

namespace fuujin {
    class Camera {
    public:
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

        glm::mat4 CalculateViewProjection(const glm::vec3& translation, const glm::quat& rotation);

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
