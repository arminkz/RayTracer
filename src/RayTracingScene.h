#pragma once 

#include "stdafx.h"
#include "VulkanContext.h"
#include "Scene.h"


class RayTracingScene : public Scene
{
public:
    RayTracingScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);
    ~RayTracingScene();

    void update(uint32_t currentImage) override;
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t targetSwapImageIndex) override;

private:

    void createBottomLevelAccelerationStructures();
    void createTopLevelAccelerationStructure();
};