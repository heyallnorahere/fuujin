#pragma once

#include "fuujin/core/Buffer.h"
#include "fuujin/renderer/Shader.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    class ShaderBuffer {
    public:
        struct FieldExpression {
            std::string Field;
            std::vector<size_t> Indices;
        };

        struct RecursiveFieldInfo {
            size_t TotalOffset, Size;
        };

        static bool ParseFieldExpresssion(const std::string& expression, FieldExpression& data);
        static bool FindField(const std::string& identifier, const std::shared_ptr<GPUType>& type,
                              RecursiveFieldInfo& field, size_t currentOffset = 0);

        ShaderBuffer() = default;
        ~ShaderBuffer() = default;

        ShaderBuffer(const std::shared_ptr<GPUType>& type);

        // we dont care about copying
        ShaderBuffer(const ShaderBuffer&) = default;
        ShaderBuffer& operator=(const ShaderBuffer&) = default;

        bool SetData(const std::string& name, const Buffer& data);
        bool GetData(const std::string& name, Buffer& data) const;

        template <typename _Ty>
        bool Set(const std::string& name, const _Ty& value) {
            ZoneScoped;

            auto data = Buffer::Wrapper(&value, sizeof(_Ty));
            return SetData(name, data);
        }

        bool Set(const std::string& name, const glm::mat4& value) {
            ZoneScoped;

            glm::mat4 matrix = value;
            if (Renderer::GetAPI().TransposeMatrices) {
                matrix = glm::transpose(matrix);
            }

            auto data = Buffer::Wrapper(&matrix, sizeof(glm::mat4));
            return SetData(name, data);
        }

        template <typename _Ty>
        bool Get(const std::string& name, _Ty& value) const {
            ZoneScoped;

            auto data = Buffer::Wrapper(&value, sizeof(_Ty));
            return GetData(name, data);
        }

        bool Get(const std::string& name, glm::mat4& value) const {
            ZoneScoped;

            auto data = Buffer::Wrapper(&value, sizeof(glm::mat4));
            if (!GetData(name, data)) {
                return false;
            }

            if (Renderer::GetAPI().TransposeMatrices) {
                value = glm::transpose(value);
            }

            return true;
        }

        Buffer& GetBuffer() { return m_Buffer; }
        const Buffer& GetBuffer() const { return m_Buffer; }

    private:
        Buffer m_Buffer;
        std::shared_ptr<GPUType> m_Type;
    };
} // namespace fuujin