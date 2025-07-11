#include "fuujinpch.h"
#include "fuujin/animation/Animation.h"

#include <fstream>

namespace YAML {
    template <>
    struct convert<fuujin::Animation::Behavior> {
        using Behavior = fuujin::Animation::Behavior;

        static Node encode(const Behavior& behavior) {
            std::string name;
            switch (behavior) {
            case Behavior::Default:
                name = "Default";
                break;
            case Behavior::Constant:
                name = "Constant";
                break;
            case Behavior::Linear:
                name = "Linear";
                break;
            default:
                throw std::runtime_error("Invalid channel behavior!");
            }

            return Node(name);
        }

        static bool decode(const Node& node, Behavior& behavior) {
            static const std::unordered_map<std::string, Behavior> nameMap = {
                { "Default", Behavior::Default },
                { "Constant", Behavior::Constant },
                { "Linear", Behavior::Linear }
            };

            auto name = node.as<std::string>();
            if (!nameMap.contains(name)) {
                return false;
            }

            behavior = nameMap.at(name);
            return true;
        }
    };
} // namespace YAML

namespace fuujin {
    static const std::vector<std::string> s_AnimationExtensions = { "anim" };

    template <typename _Ty>
    static std::optional<size_t> FindKeyframeIndex(
        Duration time, const std::vector<Animation::Keyframe<_Ty>>& keyframes) {
        ZoneScoped;

        for (size_t i = 0; i < keyframes.size(); i++) {
            const auto& keyframe = keyframes[i];
            if (time >= keyframe.Time) {
                return i;
            }
        }

        return {};
    }

    template <typename _Ty>
    static std::optional<_Ty> InterpolateKeyframes(
        Duration time, const std::vector<Animation::Keyframe<_Ty>>& keyframes,
        Animation::Behavior preBehavior, Animation::Behavior postBehavior) {
        ZoneScoped;

        if (keyframes.empty()) {
            return {};
        }

        size_t keyframeCount = keyframes.size();
        if (keyframeCount == 1) {
            return keyframes[0].Value;
        }

        Duration t0, t1;
        size_t index0 = 0;

        std::optional<size_t> index = FindKeyframeIndex(time, keyframes);
        if (!index.has_value()) {
            switch (preBehavior) {
            case Animation::Behavior::Default:
                return {};
            case Animation::Behavior::Constant:
                return keyframes[0].Value;
            }
        } else {
            size_t index0 = index.value();
        }

        bool extrapolateForward = false;
        if (index0 == keyframeCount - 1) {
            switch (postBehavior) {
            case Animation::Behavior::Default:
                return {};
            case Animation::Behavior::Constant:
                return keyframes[keyframeCount - 1].Value;
            case Animation::Behavior::Linear:
                index0--;
                extrapolateForward = true;
                break;
            default:
                throw std::runtime_error("Invalid animation behavior!");
            }
        }

        const auto& f0 = keyframes[index0];
        const auto& f1 = keyframes[index0 + 1];

        t0 = f0.Time;
        t1 = f1.Time;

        auto span = t1 - t0;
        auto interpolationTime = time;

        if (extrapolateForward) {
            interpolationTime += span;
        }

        auto t = (interpolationTime - t0) / span;
        return glm::mix(f0.Value, f1.Value, (float)t);
    }

    std::optional<glm::mat4> Animation::InterpolateChannel(Duration time, const Channel& channel) {
        ZoneScoped;

        auto translation = InterpolateKeyframes(time, channel.TranslationKeys, channel.PreBehavior,
                                                channel.PostBehavior);

        auto rotation = InterpolateKeyframes(time, channel.RotationKeys, channel.PreBehavior,
                                             channel.PostBehavior);

        auto scale = InterpolateKeyframes(time, channel.ScaleKeys, channel.PreBehavior,
                                          channel.PostBehavior);

        if (!translation.has_value() || !rotation.has_value() || !scale.has_value()) {
            // todo: default values. dont know
            return {};
        }

        auto identityMat = glm::mat4(1.f);
        auto translationMat = glm::translate(identityMat, translation.value());
        auto rotationMat = glm::toMat4(rotation.value());
        auto scaleMat = glm::scale(identityMat, scale.value());

        return translationMat * rotationMat * scaleMat;
    }

    Animation::Animation(const fs::path& path, Duration duration,
                         const std::vector<Channel>& channels) {
        ZoneScoped;

        m_Path = path;

        m_Duration = duration;
        m_Channels = channels;
    }

    template <typename _Ty>
    static void DeserializeKeyframes(const YAML::Node& node,
                                     std::vector<Animation::Keyframe<_Ty>>& keyframes) {
        ZoneScoped;

        for (const auto& keyframeNode : node) {
            auto& keyframe = keyframes.emplace_back();
            keyframe.Time = node["Time"].as<Duration>();
            keyframe.Value = node["Value"].as<_Ty>();
        }
    }

    Ref<Asset> AnimationSerializer::Deserialize(const fs::path& path) const {
        ZoneScoped;

        auto pathText = path.string();

        std::ifstream file(path);
        if (!file.is_open()) {
            FUUJIN_ERROR("Failed to open file: {}", pathText.c_str());
            return nullptr;
        }

        auto node = YAML::Load(file);
        file.close();

        FUUJIN_INFO("Deserializing animation at path: {}", pathText.c_str());

        std::vector<Animation::Channel> channels;
        const auto& channelsNode = node["Channels"];

        if (channelsNode.IsDefined() && channelsNode.IsSequence()) {
            for (const auto& channelNode : channelsNode) {
                auto& channel = channels.emplace_back();
                channel.Name = channelNode["Name"].as<std::string>();
                channel.PreBehavior = channelNode["PreBehavior"].as<Animation::Behavior>();
                channel.PostBehavior = channelNode["PostBehavior"].as<Animation::Behavior>();

                DeserializeKeyframes(channelNode["Translation"], channel.TranslationKeys);
                DeserializeKeyframes(channelNode["Rotation"], channel.RotationKeys);
                DeserializeKeyframes(channelNode["Scale"], channel.ScaleKeys);
            }
        }

        if (channels.empty()) {
            FUUJIN_ERROR("Animation has no channels - aborting");
            return nullptr;
        }

        auto duration = node["Duration"].as<Duration>();
        return Ref<Animation>::Create(path, duration, channels);
    }

    template <typename _Ty>
    static YAML::Node SerializeKeyframes(const std::vector<Animation::Keyframe<_Ty>>& keyframes) {
        ZoneScoped;
        YAML::Node keyframesNode;

        for (const auto& keyframe : keyframes) {
            YAML::Node keyframeNode;

            keyframeNode["Time"] = keyframe.Time;
            keyframeNode["Value"] = keyframe.Value;

            keyframesNode.push_back(keyframeNode);
        }

        return keyframesNode;
    }

    bool AnimationSerializer::Serialize(const Ref<Asset>& asset) const {
        ZoneScoped;

        if (asset->GetAssetType() != AssetType::Animation) {
            return false;
        }

        auto path = asset->GetPath();
        auto pathText = path.string();
        FUUJIN_INFO("Serializing animation to path: {}", pathText.c_str());

        auto animation = asset.As<Animation>();
        const auto& channels = animation->GetChannels();

        YAML::Node channelsNode;
        for (const auto& channel : channels) {
            YAML::Node channelNode;
            channelNode["Name"] = channel.Name;
            channelNode["PreBehavior"] = channel.PreBehavior;
            channelNode["PostBehavior"] = channel.PostBehavior;

            channelNode["Translation"] = SerializeKeyframes(channel.TranslationKeys);
            channelNode["Rotation"] = SerializeKeyframes(channel.RotationKeys);
            channelNode["Scale"] = SerializeKeyframes(channel.ScaleKeys);

            channelsNode.push_back(channelNode);
        }

        YAML::Node node;
        node["Duration"] = animation->GetDuration();
        node["Channels"] = channelsNode;

        std::ofstream file(path);
        if (!file.is_open()) {
            FUUJIN_ERROR("Failed to open path: {}", pathText.c_str());
            return false;
        }

        file << YAML::Dump(node);
        file.close();

        return true;
    }

    const std::vector<std::string>& AnimationSerializer::GetExtensions() const {
        return s_AnimationExtensions;
    }
} // namespace fuujin