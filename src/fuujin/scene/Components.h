#pragma once

#include "fuujin/scene/Camera.h"
#include "fuujin/scene/Transform.h"

#include "fuujin/renderer/Model.h"

namespace fuujin {
    struct TagComponent {
        std::string Tag;
    };

    struct TransformComponent {
        Transform Data;
    };

    struct CameraComponent {
        CameraComponent() { MainCamera = false; }

        Camera CameraInstance;
        bool MainCamera;
    };

    struct ModelComponent {
        Ref<Model> RenderedModel;
    };
} // namespace fuujin
