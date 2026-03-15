#pragma once

#include "scene/SceneContent.h"
#include "scene/SceneGraph.h"

class TeapotScene : public SceneContent {
public:
    using SceneContent::SceneContent;

    void onLoad() override;
    void buildUI() override;

private:
    int _materialCombo = 0;  // 0=Metallic  1=Diffuse  2=Glass  3=Marble

    void populate(SceneGraph& graph);
    void applyMaterial(SceneGraph::SceneObject& obj) const;
};
