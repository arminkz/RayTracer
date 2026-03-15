#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"

class SceneGraph;

class SceneContent {
public:
    SceneContent(std::shared_ptr<VulkanContext> ctx, SceneGraph& graph)
        : _ctx(std::move(ctx)), _graph(&graph) {}
    virtual ~SceneContent() = default;

    // Called once when scene is loaded — populate the scene graph with objects
    virtual void onLoad() = 0;

    // Called once when scene is unloaded — optional cleanup of scene-owned resources
    virtual void onUnload() {}

    // Called every frame — implement animations and transform updates here
    virtual void update(float dt) {}

    // Build ImGui controls specific to this scene
    virtual void buildUI() {}

protected:
    std::shared_ptr<VulkanContext> _ctx;
    SceneGraph* _graph;
};
