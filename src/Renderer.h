#pragma once

#include "stdafx.h"
#include "VulkanHelper.h"
#include "SwapChain.h"
#include "Scene.h"

class Renderer {

public:
    Renderer(std::shared_ptr<VulkanContext> ctx);
    ~Renderer();

    void initialize();
    void drawFrame();

    void handleMouseClick(float mx, float my) { _scene->handleMouseClick(mx, my); }
    void handleMouseDrag(float dx, float dy) { _scene->handleMouseDrag(dx, dy); }
    void handleMouseWheel(float dy) { _scene->handleMouseWheel(dy); }
    void handleKeyDown(int key, int scancode, int mods) { _scene->handleKeyDown(key, scancode, mods); }

private:
    std::shared_ptr<VulkanContext> _ctx;

    // Scene
    std::unique_ptr<Scene> _scene = nullptr;

    // Swapchain
    std::shared_ptr<SwapChain> _swapChain;

    // Command buffers
    std::vector<VkCommandBuffer> _commandBuffers;
    void createCommandBuffers();

    // Sync objects
    std::vector<VkSemaphore> _imageAvailableSemaphores;
    std::vector<VkSemaphore> _renderFinishedSemaphores;
    std::vector<VkFence> _inFlightFences;
    void createSyncObjects();


    uint32_t _frameCounter = 0;
    uint32_t _imageCounter = 0;


    // Called when the window is resized
    void invalidate();
};