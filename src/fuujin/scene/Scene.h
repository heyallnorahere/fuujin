#pragma once
#include "fuujin/core/Ref.h"

#include <entt/entt.hpp>

namespace fuujin {
    class Scene : public RefCounted {
    public:
        class Entity {
        public:
            Entity(entt::entity id, const Ref<Scene>& scene) : m_ID(id), m_Scene(scene) {}
            ~Entity() = default;

            template <typename _Ty, typename... Args>
            _Ty& AddComponent(Args&&... args);

            template <typename _Ty>
            _Ty& GetComponent();

            template <typename _Ty>
            const _Ty& GetComponent() const;

            template <typename... _Ty>
            bool HasAny() const;

            template <typename... _Ty>
            bool HasAll() const;

        private:
            entt::entity m_ID;
            Ref<Scene> m_Scene;

            friend class ::std::hash<Entity>;
        };

        Scene();
        virtual ~Scene() override;

        uint64_t GetID() const { return m_ID; }

        Entity Create(const std::optional<std::string>& tag = {});

    private:
        entt::registry m_Registry;
        uint64_t m_ID;

        friend class Scene::Entity;
    };

    template <typename _Ty, typename... Args>
    inline _Ty& Scene::Entity::AddComponent(Args&&... args) {
        ZoneScoped;

        auto& registry = m_Scene->m_Registry;
        return registry.emplace<_Ty>(m_ID, std::forward<Args>(args)...);
    }

    template <typename _Ty>
    inline _Ty& Scene::Entity::GetComponent() {
        ZoneScoped;

        auto& registry = m_Scene->m_Registry;
        return registry.get<_Ty>(m_ID);
    }

    template <typename _Ty>
    inline const _Ty& Scene::Entity::GetComponent() const {
        ZoneScoped;

        auto& registry = m_Scene->m_Registry;
        return registry.get<_Ty>(m_ID);
    }

    template <typename... _Ty>
    inline bool Scene::Entity::HasAny() const {
        ZoneScoped;

        auto& registry = m_Scene->m_Registry;
        return registry.any_of<_Ty...>(m_ID);
    }

    template <typename... _Ty>
    inline bool Scene::Entity::HasAll() const {
        ZoneScoped;

        auto& registry = m_Scene->m_Registry;
        return registry.all_of<_Ty...>(m_ID);
    }
} // namespace fuujin

namespace std {
    template <>
    struct hash<::fuujin::Scene::Entity> {
        using Entity = ::fuujin::Scene::Entity;

        size_t operator()(const Entity& entity) const {
            static constexpr size_t bitSize = sizeof(size_t) * 8;

            size_t hash = 0;

            hash ^= entity.m_Scene->GetID();
            hash ^= (uint64_t)entity.m_ID << (bitSize / 2);

            return hash;
        }
    };
} // namespace std
