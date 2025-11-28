#include "SwapChain.h"


SwapChain::SwapChain(std::shared_ptr<VulkanContext> ctx)
 : _ctx(ctx) 
{
    createSwapChain();
}


SwapChain::~SwapChain() {
    cleanupSwapChain();
}

bool SwapChain::firstTimeCreation = true;

void SwapChain::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = VulkanHelper::querySwapChainSupport(_ctx->physicalDevice, _ctx->surface);
    
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
    
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    if (firstTimeCreation) spdlog::info("Swap chain image count: {}", imageCount);
    
    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = _ctx->surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    QueueFamilyIndices indices = VulkanHelper::findQueueFamilies(_ctx->physicalDevice, _ctx->surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily) {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        if (firstTimeCreation) spdlog::info("Swap chain sharing mode: concurrent (graphics and present families are different)");
    } else {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (firstTimeCreation) spdlog::info("Swap chain sharing mode: exclusive (graphics and present families are the same)");
    }
    
    swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    
    if (vkCreateSwapchainKHR(_ctx->device, &swapChainCreateInfo, nullptr, &_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }
    if (firstTimeCreation) spdlog::info("Swap chain created successfully.");
    
    vkGetSwapchainImagesKHR(_ctx->device, _swapChain, &imageCount, nullptr);
    _swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_ctx->device, _swapChain, &imageCount, _swapChainImages.data());
    
    _swapChainImageFormat = surfaceFormat.format;
    if (firstTimeCreation) spdlog::info("Swap chain image format: {}", VulkanHelper::formatToString(_swapChainImageFormat));
    
    _swapChainExtent = extent;
    
    _swapChainImageViews.resize(_swapChainImages.size());
    for (size_t i = 0; i < _swapChainImages.size(); i++) {
        // Create image views for each swap chain image
        _swapChainImageViews[i] = VulkanHelper::createImageView(_ctx, _swapChainImages[i], _swapChainImageFormat, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    firstTimeCreation = false;
}


void SwapChain::cleanupSwapChain() {
    for (size_t i = 0; i < _swapChainImageViews.size(); i++) {
        vkDestroyImageView(_ctx->device, _swapChainImageViews[i], nullptr);
    }
    vkDestroySwapchainKHR(_ctx->device, _swapChain, nullptr);
}


VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Check for mailbox mode first (triple buffering)
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    // Fallback to FIFO mode (double buffering)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        SDL_GetWindowSizeInPixels(_ctx->window, &width, &height);
        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }
}