#include "fuujinpch.h"
#include "fuujin/renderer/ShaderBuffer.h"

#include <regex>
#include <sstream>

namespace fuujin {
    bool ShaderBuffer::ParseFieldExpresssion(const std::string& expression, FieldExpression& data) {
        ZoneScoped;

        static const std::regex exprRegex("^([A-Za-z][0-9A-Za-z]*)((\\[[0-9]+\\])*)$");
        std::smatch exprMatch;
        if (!std::regex_match(expression, exprMatch, exprRegex)) {
            return false;
        }

        data.Field = exprMatch[1].str();

        std::string indicesString = exprMatch[2].str();
        static constexpr char openIndex = '[';
        static constexpr char closeIndex = ']';

        size_t currentPos = indicesString.find(openIndex);
        while (currentPos != std::string::npos) {
            size_t closingBracket = indicesString.find(closeIndex, currentPos);
            size_t indexBegin = currentPos + 1;

            std::string indexString = indicesString.substr(indexBegin, closingBracket - indexBegin);
            std::stringstream ss(indexString);
            ss >> data.Indices.emplace_back();

            currentPos = indicesString.find(openIndex, indexBegin);
        }

        return true;
    }

    bool ShaderBuffer::FindField(const std::string& identifier,
                                 const std::shared_ptr<GPUType>& type, RecursiveFieldInfo& field,
                                 size_t currentOffset) {
        ZoneScoped;

        std::string child, current;
        size_t delimiter = identifier.find('.');
        if (delimiter != std::string::npos) {
            child = identifier.substr(delimiter + 1);
            current = identifier.substr(0, delimiter);
        } else {
            current = identifier;
        }

        FieldExpression expression;
        if (!ParseFieldExpresssion(current, expression)) {
            FUUJIN_WARN("Invalid field expression: {}", current.c_str());
            return false;
        }

        const auto& fields = type->GetFields();
        if (!fields.contains(expression.Field)) {
            FUUJIN_WARN("Cannot find field {} on shader type {}", expression.Field.c_str(),
                        type->GetName().c_str());

            return false;
        }

        const auto& fieldData = fields.at(expression.Field);
        size_t newOffset = fieldData.Offset + currentOffset;

        if (expression.Indices.size() > fieldData.Dimensions.size()) {
            FUUJIN_ERROR("Attempted to specify {} indices - array is {}-dimensional",
                         expression.Indices.size(), fieldData.Dimensions.size());

            return false;
        }

        if (!fieldData.Dimensions.empty()) {
            std::stack<size_t> strides;
            strides.push(fieldData.Stride);

            for (size_t i = fieldData.Dimensions.size() - 1; i > 0; i--) {
                size_t previous = strides.top();
                size_t dimension = fieldData.Dimensions[i];

                strides.push(previous * dimension);
            }

            for (size_t index : expression.Indices) {
                size_t stride = strides.top();
                newOffset += stride * index;

                strides.pop();
            }
        }

        if (child.empty()) {
            field.TotalOffset = newOffset;
            field.Type = fieldData.Type;

            return true;
        } else {
            return FindField(child, fieldData.Type, field, newOffset);
        }
    }

    ShaderBuffer::ShaderBuffer(const std::shared_ptr<GPUType>& type) {
        ZoneScoped;

        m_Type = type;
        m_Buffer = Buffer(type->GetSize());
    }

    ShaderBuffer::ShaderBuffer(ShaderBuffer&& other) {
        ZoneScoped;

        m_Buffer = Buffer(std::move(other.m_Buffer));
        m_Type = other.m_Type;
    }

    ShaderBuffer& ShaderBuffer::operator=(ShaderBuffer&& other) {
        ZoneScoped;

        m_Buffer = Buffer(std::move(other.m_Buffer));
        m_Type = other.m_Type;

        return *this;
    }

    bool ShaderBuffer::SetData(const std::string& name, const Buffer& data) {
        ZoneScoped;

        RecursiveFieldInfo field;
        if (!FindField(name, m_Type, field)) {
            return false;
        }

        auto slice = m_Buffer.Slice(field.TotalOffset, field.Type->GetSize());
        Buffer::Copy(data, slice, data.GetSize());

        return true;
    }

    bool ShaderBuffer::GetData(const std::string& name, Buffer& data) const {
        ZoneScoped;

        RecursiveFieldInfo field;
        if (!FindField(name, m_Type, field)) {
            return false;
        }

        auto slice = m_Buffer.Slice(field.TotalOffset, field.Type->GetSize());
        Buffer::Copy(slice, data, data.GetSize());

        return true;
    }

    bool ShaderBuffer::Slice(const std::string& name, ShaderBuffer& slice) {
        ZoneScoped;

        RecursiveFieldInfo field;
        if (!FindField(name, m_Type, field)) {
            return false;
        }

        auto bufferSlice = m_Buffer.Slice(field.TotalOffset, field.Type->GetSize());
        slice = ShaderBuffer(std::move(bufferSlice), field.Type);

        return true;
    }

    ShaderBuffer::ShaderBuffer(Buffer&& slice, const std::shared_ptr<GPUType>& type) {
        ZoneScoped;

        m_Buffer = Buffer(std::move(slice));
        m_Type = type;
    }
} // namespace fuujin
