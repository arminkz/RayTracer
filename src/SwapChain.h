#pragma once

#include "stdafx.h"
#include "VulkanContext.h"


class SwapChain {
public:
    SwapChain(std::shared_ptr<VulkanContext> ctx);
    ~SwapChain();

    void createSwapChain();
    void cleanupSwapChain();

    VkSwapchainKHR getSwapChain() const { return _swapChain; }
    VkFormat getSwapChainImageFormat() const { return _swapChainImageFormat; }
    VkExtent2D getSwapChainExtent() const { return _swapChainExtent; }
    const std::vector<VkImage>& getSwapChainImages() const { return _swapChainImages; }
    const std::vector<VkImageView>& getSwapChainImageViews() const { return _swapChainImageViews; }
    const int getSwapChainImageCount() const { return static_cast<int>(_swapChainImages.size()); }

private:
    std::shared_ptr<VulkanContext> _ctx;

    VkSwapchainKHR _swapChain = nullptr;
    VkFormat _swapChainImageFormat;
    VkExtent2D _swapChainExtent;
    std::vector<VkImage> _swapChainImages;
    std::vector<VkImageView> _swapChainImageViews;

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    static bool firstTimeCreation;
};