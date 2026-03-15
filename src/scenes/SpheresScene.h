#pragma once

#include "SceneContent.h"

class SpheresScene : public SceneContent {
public:
    using SceneContent::SceneContent;

    void onLoad() override;
    void buildUI() override;
};
