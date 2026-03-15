#pragma once
#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/SwapChain.h"

class Renderer
{
public:
    Renderer(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
        : _ctx(std::move(ctx)), _swapChain(std::move(swapChain)) {}
    virtual ~Renderer() = default;

    virtual void update(uint32_t currentImage) { _currentFrame = currentImage; }
    virtual void recordToCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) = 0;
    virtual void onSwapChainRecreated() {}

    virtual VkImage getOutputImage() const { return VK_NULL_HANDLE; }
    virtual VkExtent2D getOutputExtent() const { return {}; }

    // IO handling
    virtual void handleMouseClick(float mx, float my) = 0;
    virtual void handleMouseDrag(float dx, float dy) = 0;
    virtual void handleMouseWheel(float dy) = 0;
    virtual void handleKeyDown(int key, int scancode, int mods) = 0;

    // Called each frame to build scene-specific ImGui controls
    virtual void buildUI() {}

protected:
    std::shared_ptr<VulkanContext> _ctx;
    std::shared_ptr<SwapChain> _swapChain;
    uint32_t _currentFrame = 0;
};
