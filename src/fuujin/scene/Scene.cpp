#include "fuujinpch.h"
#include "fuujin/scene/Scene.h"

#include "fuujin/scene/Components.h"

namespace fuujin {
    static uint64_t s_SceneID = 0;

    Scene::Scene() {
        ZoneScoped;

        m_ID = s_SceneID++;
    }

    Scene::~Scene() {
        ZoneScoped;

        // todo: destroy?
    }

    Scene::Entity Scene::Create(const std::optional<std::string>& tag) {
        ZoneScoped;

        entt::entity handle = m_Registry.create();
        Entity entity(handle, this);

        auto& tc = entity.AddComponent<TagComponent>();
        tc.Tag = tag.value_or("<no tag>");

        return entity;
    }
} // namespace fuujin
