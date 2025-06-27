#include "fuujinpch.h"
#include "fuujin/renderer/Material.h"

#include "fuujin/renderer/Renderer.h"

namespace fuujin {
    static uint64_t s_MaterialID = 0;

    // see assets/shaders/include/Material.glsl
    std::string Material::GetPropertyFieldName(Property id) {
        switch (id) {
        case Property::AlbedoColor:
            return "Albedo";
        case Property::SpecularColor:
            return "Specular";
        case Property::AmbientColor:
            return "Ambient";
        case Property::Shininess:
            return "Shininess";
        case Property::HasNormalMap:
            return "HasNormalMap";
        default:
            throw std::runtime_error("Invalid property ID");
        }
    }

    // see assets/shaders/include/Material.glsl
    std::string Material::GetTextureName(TextureSlot slot) {
        switch (slot) {
        case TextureSlot::Albedo:
            return "u_Albedo";
            break;
        case TextureSlot::Specular:
            return "u_Specular";
            break;
        case TextureSlot::Ambient:
            return "u_Ambient";
            break;
        case TextureSlot::Normal:
            return "u_Normal";
            break;
        default:
            throw std::runtime_error("Invalid texture slot!");
        }
    }


    Material::Material() {
        ZoneScoped;

        m_ID = s_MaterialID++;
        m_State = 0;

        static const glm::vec4 white = glm::vec4(1.f);
        SetProperty(Property::AlbedoColor, white);
        SetProperty(Property::SpecularColor, white);
        SetProperty(Property::AmbientColor, white);
        SetProperty(Property::Shininess, 256.f);
        SetProperty(Property::HasNormalMap, false);

        m_Pipeline.Wireframe = false;
    }

    Material::~Material() {
        ZoneScoped;
        Renderer::FreeMaterial(m_ID);
    }

    void Material::SetTexture(TextureSlot slot, const Ref<Texture>& texture) {
        ZoneScoped;

        m_Textures[slot] = texture;
        switch (slot) {
        case TextureSlot::Normal:
            SetProperty(Property::HasNormalMap, true);
            break;
        default:
            Invalidate();
            break;
        }
    }

    void Material::SetPropertyData(Property name, const Buffer& data) {
        ZoneScoped;

        if (m_PropertyData.contains(name)) {
            size_t previousSize = m_PropertyData.at(name).GetSize();
            if (previousSize != data.GetSize()) {
                throw std::runtime_error("Property data size mismatch!");
            }
        }

        m_PropertyData[name] = data.Copy();
        Invalidate();
    }

    void Material::MapProperties(const std::shared_ptr<GPUResource>& resource,
                                 Buffer& buffer) const {
        ZoneScoped;

        std::unordered_map<std::string, GPUType::Field> fields;
        std::shared_ptr<GPUType> type = resource->GetType();
        type->GetFields(fields);

        for (const auto& [id, data] : m_PropertyData) {
            auto fieldName = GetPropertyFieldName(id);
            if (!fields.contains(fieldName)) {
                FUUJIN_WARN("Material property field not found in buffer: {}", fieldName.c_str());
                continue;
            }

            const auto& field = fields.at(fieldName);
            size_t fieldSize = field.Type->GetSize();

            // all buffer operations perform bounds checks
            auto slice = buffer.Slice(field.Offset, fieldSize);
            Buffer::Copy(data, slice, data.GetSize());
        }
    }

    void Material::SetPipelineSpec(Pipeline::Spec& spec) const {
        ZoneScoped;

        spec.Wireframe = m_Pipeline.Wireframe;
        // todo: more properties
    }
} // namespace fuujin