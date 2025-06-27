#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/core/Buffer.h"

#include "fuujin/renderer/Texture.h"
#include "fuujin/renderer/Shader.h"
#include "fuujin/renderer/Pipeline.h"

namespace fuujin {
    class Material : public RefCounted {
    public:
        enum class TextureSlot : uint32_t { Albedo = 0, Specular, Ambient, Normal, MAX };

        enum class Property : uint32_t {
            AlbedoColor = 0,
            SpecularColor,
            AmbientColor,
            Shininess,
            HasNormalMap,

            MAX
        };

        struct PipelineProperties {
            bool Wireframe;
            // todo: others
        };

        static std::string GetPropertyFieldName(Property id);
        static std::string GetTextureName(TextureSlot slot);

        Material();
        virtual ~Material() override;

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        uint64_t GetID() const { return m_ID; }
        uint64_t GetState() const { return m_State; }

        // increments the material's "state"
        // this prompts all systems using this material to invalidate their caches and device
        // objects
        void Invalidate() { m_State++; }

        void SetTexture(TextureSlot slot, const Ref<Texture>& texture);
        const std::map<TextureSlot, Ref<Texture>>& GetTextures() const { return m_Textures; }

        PipelineProperties& GetPipeline() { return m_Pipeline; }
        const PipelineProperties& GetPipeline() const { return m_Pipeline; }

        void SetPropertyData(Property name, const Buffer& data);

        template <typename _Ty>
        void SetProperty(Property name, const _Ty& data) {
            auto wrapper = Buffer::Wrapper(&data, sizeof(_Ty));
            SetPropertyData(name, wrapper);
        }

        void MapProperties(const std::shared_ptr<GPUResource>& resource, Buffer& buffer) const;
        void SetPipelineSpec(Pipeline::Spec& spec) const;

    private:
        uint64_t m_ID, m_State;

        std::map<TextureSlot, Ref<Texture>> m_Textures;
        std::unordered_map<Property, Buffer> m_PropertyData;
        PipelineProperties m_Pipeline;
    };
} // namespace fuujin