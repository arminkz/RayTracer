#pragma once

#include "scene/SceneContent.h"

class GlassAndSphereScene : public SceneContent {
public:
    using SceneContent::SceneContent;

    void onLoad() override;
    void buildUI() override;
};
