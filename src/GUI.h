#pragma once

#include "stdafx.h"
#include "VulkanContext.h"

// Forward declare ImDrawData to avoid including imgui.h here
struct ImDrawData;

class GUI {
public:
    GUI(std::shared_ptr<VulkanContext> ctx, SDL_Window* window, VkFormat swapChainImageFormat);
    ~GUI();

    // Called once per frame before recordCommandBuffer — builds CPU-side draw data
    void beginFrame();
    void buildUI();

    // Called inside recordCommandBuffer — records ImGui draw commands into the command buffer
    void recordToCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkExtent2D extent);

    VkRenderPass getRenderPass() const { return _renderPass; }

    // Pass SDL events to ImGui; returns true if ImGui wants to handle the event
    bool handleEvent(SDL_Event* event);

    // True when ImGui is consuming mouse/keyboard input
    bool isCapturingMouse() const;
    bool isCapturingKeyboard() const;

private:
    std::shared_ptr<VulkanContext> _ctx;
    SDL_Window* _window;

    VkDescriptorPool _imguiDescriptorPool = VK_NULL_HANDLE;
    VkRenderPass _renderPass = VK_NULL_HANDLE;

    void createDescriptorPool();
    void createRenderPass(VkFormat swapChainImageFormat);
};
