#include "fuujinpch.h"
#include "fuujin/renderer/Material.h"

#include "fuujin/renderer/Renderer.h"
#include "fuujin/renderer/ShaderBuffer.h"

#include "fuujin/asset/AssetManager.h"

#include <fstream>

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

    template <>
    struct convert<fuujin::Material::Property> {
        using Property = fuujin::Material::Property;

        static Node encode(const Property& property) {
            auto name = fuujin::Material::GetPropertyFieldName(property);
            return Node(name);
        }

        static bool decode(const Node& node, Property& property) {
            static const std::unordered_map<std::string, Property> nameToProperty = {
                { "Albedo", Property::AlbedoColor },
                { "Specular", Property::SpecularColor },
                { "Ambient", Property::AmbientColor },
                { "Shininess", Property::Shininess }
            };

            auto propertyName = node.as<std::string>();
            if (!nameToProperty.contains(propertyName)) {
                return false;
            }

            property = nameToProperty.at(propertyName);
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

        m_Pipeline.Wireframe = false;
    }

    Material::~Material() {
        ZoneScoped;
        Renderer::FreeMaterial(m_ID);
    }

    void Material::SetTexture(TextureSlot slot, const Ref<Texture>& texture) {
        ZoneScoped;

        m_Textures[slot] = texture;
        Invalidate();
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

        bool hasNormalMap = m_Textures.contains(TextureSlot::Normal);
        buffer.Set("HasNormalMap", hasNormalMap);
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

        std::ifstream stream(path);
        if (!stream.is_open()) {
            FUUJIN_ERROR("Failed to open path {}", pathText.c_str());
            return nullptr;
        }

        auto node = YAML::Load(stream);
        auto material = Ref<Material>::Create(path);

        stream.close();

        auto textureNode = node["Textures"];
        if (textureNode.IsSequence()) {
            for (auto data : textureNode) {
                auto slot = data["Slot"].as<Material::TextureSlot>();
                fs::path virtualPath = data["Asset"].as<std::string>();

                auto key = AssetManager::NormalizePath(virtualPath);
                auto texture = AssetManager::GetAsset<Texture>(key);

                auto name = Material::GetTextureName(slot);
                auto virtualText = key.string();
                if (texture) {
                    material->SetTexture(slot, texture);
                    FUUJIN_DEBUG("Set material texture {} to asset at virtual path {}",
                                 name.c_str(), virtualText.c_str());
                } else {
                    FUUJIN_ERROR("Failed to find {} texture of virtual path {} - skipping",
                                 name.c_str(), virtualText.c_str());
                }
            }
        } else {
            FUUJIN_DEBUG("No textures specified - skipping texture search");
        }

        auto propertyNode = node["Properties"];
        if (propertyNode.IsSequence()) {
            for (auto data : propertyNode) {
                auto name = data["Name"].as<Material::Property>();

                auto valueNode = data["Value"];
                std::vector<float> values;
                if (valueNode.IsSequence()) {
                    values = valueNode.as<std::vector<float>>();
                } else if (valueNode.IsScalar()) {
                    values = { valueNode.as<float>() };
                } else {
                    throw std::runtime_error("Invalid property node!");
                }

                material->SetPropertyData(name, Buffer::Wrapper(values));
            }
        } else {
            FUUJIN_DEBUG("No properties specified - skipping property deserialization");
        }

        auto pipelineNode = node["Pipeline"];
        if (pipelineNode.IsMap()) {
            auto& spec = material->GetPipeline();

            auto wireframeNode = pipelineNode["Wireframe"];
            if (wireframeNode.IsDefined()) {
                spec.Wireframe = wireframeNode.as<bool>();
            }
        }

        return material;
    }

    bool MaterialSerializer::Serialize(const Ref<Asset>& asset) const {
        ZoneScoped;
        if (asset->GetAssetType() != AssetType::Material) {
            return false;
        }

        auto material = asset.As<Material>();
        const auto& textures = material->GetTextures();
        const auto& properties = material->GetPropertyData();

        auto realPath = material->GetPath();
        auto realText = realPath.lexically_normal().string();
        FUUJIN_INFO("Serializing material to path {}", realText.c_str());

        YAML::Node texturesNode;
        for (const auto& [name, texture] : textures) {
            auto realPath = texture->GetPath();
            auto virtualPath = AssetManager::GetVirtualPath(realPath);

            auto realText = realPath.lexically_normal().string();
            if (!virtualPath.has_value()) {
                FUUJIN_ERROR("Could not find virtual path for texture at {}", realText.c_str());
            }

            YAML::Node textureNode;
            textureNode["Slot"] = name;
            textureNode["Asset"] = virtualPath.value().string();

            texturesNode.push_back(textureNode);
        }

        YAML::Node propertiesNode;
        for (const auto& [property, data] : properties) {
            auto name = Material::GetPropertyFieldName(property);
            if (data.IsEmpty()) {
                FUUJIN_WARN("Property {} is empty - skipping serialization", name.c_str());
                continue;
            }

            size_t dataSize = data.GetSize();
            if (dataSize % 4 != 0) {
                FUUJIN_DEBUG("Property {} is not a scalar or vector - skipping serialization",
                             name.c_str());

                continue;
            }

            size_t elements = dataSize / 4;
            auto dataPtr = (const float*)data.Get();

            YAML::Node valueNode;
            if (elements == 1) {
                valueNode = *dataPtr;
            } else {
                valueNode.SetStyle(YAML::EmitterStyle::Flow);
                for (size_t i = 0; i < elements; i++) {
                    float element = dataPtr[i];
                    valueNode.push_back(element);
                }
            }

            YAML::Node propertyNode;
            propertyNode["Name"] = property;
            propertyNode["Value"] = valueNode;

            propertiesNode.push_back(propertyNode);
        }

        YAML::Node pipelineNode;
        auto& spec = material->GetPipeline();
        pipelineNode["Wireframe"] = spec.Wireframe;

        YAML::Node node;
        node["Textures"] = texturesNode;
        node["Properties"] = propertiesNode;
        node["Pipeline"] = pipelineNode;

        if (realPath.has_parent_path()) {
            auto directory = realPath.parent_path();
            fs::create_directories(directory);
        }

        std::ofstream stream(realPath);
        if (!stream.is_open()) {
            FUUJIN_ERROR("Failed to open writing stream to path {}", realText.c_str());
            return false;
        }

        stream << YAML::Dump(node);
        stream.close();

        FUUJIN_INFO("Material serialized to {}", realText.c_str());
        return true;
    }

    const std::vector<std::string>& MaterialSerializer::GetExtensions() const {
        return s_MaterialExtensions;
    }

    AssetType MaterialSerializer::GetType() const { return AssetType::Material; }
} // namespace fuujin