#pragma once

#include "stdafx.h"
#include "vulkan/VulkanHelper.h"
#include "vulkan/SwapChain.h"
#include "core/Renderer.h"
#include "gui/GUI.h"

class FramePresenter {

public:
    FramePresenter(std::shared_ptr<VulkanContext> ctx);
    ~FramePresenter();

    void present();

    // Route an SDL event to ImGui or the Renderer
    void handleEvent(SDL_Event* event);

private:
    std::shared_ptr<VulkanContext> _ctx;

    // Renderer
    std::unique_ptr<Renderer> _renderer;

    // GUI
    std::unique_ptr<GUI> _gui;

    // Swapchain
    std::shared_ptr<SwapChain> _swapChain;

    // Command buffers
    std::vector<VkCommandBuffer> _commandBuffers;
    void createCommandBuffers();

    // FrameBuffers (used for ImGui - raytracing doesnt use framebuffers and writes to storage image instead)
    std::vector<VkFramebuffer> _framebuffers;
    void createFramebuffers();
    void destroyFramebuffers();

    // Sync objects
    std::vector<VkSemaphore> _imageAvailableSemaphores;
    std::vector<VkSemaphore> _renderFinishedSemaphores;
    std::vector<VkFence> _inFlightFences;
    void createSyncObjects();
    void destroySyncObjects();

    uint32_t _frameCounter = 0;
    uint32_t _imageCounter = 0;

    // Called when the window is resized
    void invalidate();
};
