#pragma once
#include "fuujin/asset/Asset.h"

namespace fuujin {
    class Animation : public Asset {
    public:
        enum class Behavior {
            Default,
            Constant,
            Linear
        };

        template <typename _Ty>
        struct Keyframe {
            Duration Time;
            _Ty Value;
        };

        using VectorKeyframe = Keyframe<glm::vec3>;
        using QuaternionKeyframe = Keyframe<glm::quat>;

        // mapped from aiNodeAnim
        struct Channel {
            std::vector<VectorKeyframe> TranslationKeys;
            std::vector<QuaternionKeyframe> RotationKeys;
            std::vector<VectorKeyframe> ScaleKeys;

            // maps to a node name in a model tree
            std::string Name;
            
            Behavior PreBehavior, PostBehavior;
        };

        static std::optional<glm::mat4> InterpolateChannel(Duration time, const Channel& channel);

        Animation(const fs::path& path, Duration duration, const std::vector<Channel>& channels);
        virtual ~Animation() override = default;

        Animation(const Animation&) = delete;
        Animation& operator=(const Animation&) = delete;

        virtual const fs::path& GetPath() const override { return m_Path; }
        virtual AssetType GetAssetType() const override { return AssetType::Animation; };

        const std::vector<Channel>& GetChannels() const { return m_Channels; }
        Duration GetDuration() const { return m_Duration; }

    private:
        fs::path m_Path;

        std::vector<Channel> m_Channels;
        Duration m_Duration;
    };

    template <>
    inline std::optional<AssetType> GetAssetType<Animation>() {
        return AssetType::Animation;
    }

    class AnimationSerializer : public AssetSerializer {
    public:
        virtual Ref<Asset> Deserialize(const fs::path& path) const override;
        virtual bool Serialize(const Ref<Asset>& asset) const override;

        virtual const std::vector<std::string>& GetExtensions() const override;
        virtual AssetType GetType() const override { return AssetType::Animation; }
    };
} // namespace fuujin