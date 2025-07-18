#pragma once
#include "fuujin/core/Ref.h"

#include <entt/entt.hpp>

namespace fuujin {
    class Scene : public RefCounted {
    public:
        class Entity {
        public:
            Entity() : m_ID(entt::null), m_Scene(nullptr) {}
            Entity(entt::entity id, const Ref<Scene>& scene) : m_ID(id), m_Scene(scene) {}
            ~Entity() = default;

            template <typename _Ty, typename... Args>
            _Ty& AddComponent(Args&&... args) const;

            template <typename _Ty>
            _Ty& GetComponent() const;

            template <typename... _Ty>
            bool HasAny() const;

            template <typename... _Ty>
            bool HasAll() const;

            bool IsPresent() const { return m_ID != entt::null && m_Scene.IsPresent(); }
            bool IsEmpty() const { return m_ID == entt::null || m_Scene.IsEmpty(); }

            operator bool() const { return IsPresent(); }

        private:
            entt::entity m_ID;
            Ref<Scene> m_Scene;

            friend class ::std::hash<Entity>;
        };

        Scene();
        virtual ~Scene() override;

        uint64_t GetID() const { return m_ID; }

        Entity Create(const std::optional<std::string>& tag = {});

        // callback signature:
        // void(const Scene::Entity[&], _Ty&...)
        // OR
        // bool(const Scene::Entity[&], _Ty&...)
        // a return value of true signifies "break"
        template <typename... _Ty, typename _Func>
        void View(const _Func& callback) {
            ZoneScoped;

            auto view = m_Registry.view<_Ty...>();
            for (entt::entity id : view) {
                const Entity entity(id, this);

                if constexpr (std::is_same_v<std::invoke_result_t<_Func, Entity, _Ty&...>, bool>) {
                    bool doBreak = callback(entity, std::forward<_Ty&>(m_Registry.get<_Ty>(id))...);
                    if (doBreak) {
                        break;
                    }
                } else {
                    callback(entity, std::forward<_Ty&>(m_Registry.get<_Ty>(id))...);
                }
            }
        }

    private:
        entt::registry m_Registry;
        uint64_t m_ID;

        friend class Scene::Entity;
    };

    template <typename _Ty, typename... Args>
    inline _Ty& Scene::Entity::AddComponent(Args&&... args) const {
        ZoneScoped;

        auto& registry = m_Scene->m_Registry;
        return registry.emplace<_Ty>(m_ID, std::forward<Args>(args)...);
    }

    template <typename _Ty>
    inline _Ty& Scene::Entity::GetComponent() const {
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
