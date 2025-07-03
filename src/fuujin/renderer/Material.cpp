#include "fuujinpch.h"
#include "fuujin/renderer/Material.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/ShaderBuffer.h"

#include "fuujin/asset/AssetManager.h"

namespace YAML {
    template <>
    struct convert<fuujin::Material::TextureSlot> {
        using TextureSlot = fuujin::Material::TextureSlot;

        static Node encode(const TextureSlot& slot) {
            std::string name;
            switch (slot) {
            case TextureSlot::Albedo:
                name = "Albedo";
                break;
            case TextureSlot::Specular:
                name = "Specular";
                break;
            case TextureSlot::Ambient:
                name = "Ambient";
                break;
            case TextureSlot::Normal:
                name = "Normal";
                break;
            default:
                return Node(NodeType::Null);
            }

            return Node(name);
        }

        static bool decode(const Node& node, TextureSlot& slot) {
            static const std::unordered_map<std::string, TextureSlot> nameToSlot = {
                { "Albedo", TextureSlot::Albedo },
                { "Specular", TextureSlot::Specular },
                { "Ambient", TextureSlot::Ambient },
                { "Normal", TextureSlot::Normal },
            };

            auto name = node.as<std::string>();
            if (!nameToSlot.contains(name)) {
                return false;
            }

            slot = nameToSlot.at(name);
            return true;
        }
    };
} // namespace YAML

namespace fuujin {
    static uint64_t s_MaterialID = 0;
    static const std::vector<std::string> s_MaterialExtensions = { "mat" };

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

    Material::Material(const fs::path& path) {
        ZoneScoped;

        m_ID = s_MaterialID++;
        m_State = 0;
        m_Path = path;

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

    void Material::MapProperties(ShaderBuffer& buffer) const {
        ZoneScoped;

        for (const auto& [id, data] : m_PropertyData) {
            auto fieldName = GetPropertyFieldName(id);
            if (!buffer.SetData(fieldName, data)) {
                FUUJIN_WARN("Failed to set material property: {}", fieldName.c_str());
            }
        }
    }

    void Material::SetPipelineSpec(Pipeline::Spec& spec) const {
        ZoneScoped;

        spec.Wireframe = m_Pipeline.Wireframe;
        // todo: more properties
    }

    Ref<Asset> MaterialSerializer::Deserialize(const fs::path& path) const {
        ZoneScoped;

        auto pathText = path.lexically_normal().string();
        FUUJIN_INFO("Loading material at path {}", pathText.c_str());

        YAML::Node node;
        try {
            node = YAML::LoadFile(path.string());
        } catch (const YAML::BadFile& exc) {
            return nullptr;
        }

        auto material = Ref<Material>::Create(path);
        auto textureNode = node["Textures"];
        auto propertyNode = node["Properties"];

        if (textureNode.IsSequence()) {
            for (auto data : textureNode) {
                auto slot = data["Slot"].as<Material::TextureSlot>();
                fs::path virtualPath = data["Asset"].as<std::string>();

                auto key = AssetManager::NormalizePath(virtualPath);
                auto texture = AssetManager::GetAsset<Texture>(key);

                auto name = Material::GetTextureName(slot);
                if (texture) {
                    material->SetTexture(slot, texture);
                    FUUJIN_DEBUG("Loaded texture {}", name.c_str());
                } else {
                    auto virtualText = key.lexically_normal().string();
                    FUUJIN_WARN("Failed to find {} texture of virtual path {}", name.c_str(),
                                virtualText.c_str());
                }
            }
        } else {
            FUUJIN_DEBUG("No textures specified - skipping texture search");
        }
    }
} // namespace fuujin