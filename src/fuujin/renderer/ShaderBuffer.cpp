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

        if (child.empty()) {
            field.Size = fieldData.Type->GetSize();
            field.TotalOffset = newOffset;

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

    bool ShaderBuffer::SetData(const std::string& name, const Buffer& data) {
        ZoneScoped;

        RecursiveFieldInfo field;
        if (!FindField(name, m_Type, field)) {
            return false;
        }

        auto slice = m_Buffer.Slice(field.TotalOffset, field.Size);
        Buffer::Copy(data, slice, data.GetSize());

        return true;
    }

    bool ShaderBuffer::GetData(const std::string& name, Buffer& data) const {
        ZoneScoped;

        RecursiveFieldInfo field;
        if (!FindField(name, m_Type, field)) {
            return false;
        }

        auto slice = m_Buffer.Slice(field.TotalOffset, field.Size);
        Buffer::Copy(slice, data, data.GetSize());

        return true;
    }
} // namespace fuujin