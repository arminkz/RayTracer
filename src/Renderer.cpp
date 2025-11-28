#include "Renderer.h"
#include "SwapChain.h"
#include "RayTracingScene.h"

Renderer::Renderer(std::shared_ptr<VulkanContext> ctx)
    : _ctx(std::move(ctx))
{
}


Renderer::~Renderer() {
    // Wait for any unfinished GPU tasks
    vkDeviceWaitIdle(_ctx->device);
}

void Renderer::initialize() {
    spdlog::info("Max Frames in flight: {}", MAX_FRAMES_IN_FLIGHT);

    // Create swap chain
    _swapChain = std::make_shared<SwapChain>(_ctx);

    // Initialize Scene
    _scene = std::make_unique<RayTracingScene>(_ctx, _swapChain);

    createCommandBuffers();
    createSyncObjects();
}


void Renderer::createCommandBuffers() {
    // Allocate command buffer
    _commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _ctx->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();

    if (vkAllocateCommandBuffers(_ctx->device, &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
        spdlog::error("Failed to allocate command buffers!");
    } else {
        spdlog::info("Command buffers allocated successfully");
    }
}


void Renderer::createSyncObjects() {
    _imageAvailableSemaphores.resize(_swapChain->getSwapChainImageCount());
    _renderFinishedSemaphores.resize(_swapChain->getSwapChainImageCount());
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //Initially signaled

    for (size_t i = 0; i < _swapChain->getSwapChainImageCount(); i++) {
        if (vkCreateSemaphore(_ctx->device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_ctx->device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS) {
            spdlog::error("Failed to create semaphores for swap chain image {}!", i);
        }
    }

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(_ctx->device, &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS) {
            spdlog::error("Failed to create fences for frame {}!", i);
        }
    }
}


void Renderer::invalidate() {
    // Handle swap chain recreation on window resize (TODO)
    vkDeviceWaitIdle(_ctx->device);
    spdlog::error("Window resized. (Equivalat to getting nuked in Vulkan terms) (TODO)");
}


void Renderer::drawFrame() {
    // Wait for the previous frame to finish
    vkWaitForFences(_ctx->device, 1, &_inFlightFences[_frameCounter], VK_TRUE, UINT64_MAX);

    // Wait for a swap chain image to be available
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_ctx->device, _swapChain->getSwapChain(), UINT64_MAX, _imageAvailableSemaphores[_imageCounter], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        invalidate();
        return;
    } else if (result != VK_SUCCESS) {
        spdlog::error("Failed to acquire swap chain image!");
        return;
    }

    // Reset the fence before drawing
    vkResetFences(_ctx->device, 1, &_inFlightFences[_frameCounter]);
    vkResetCommandBuffer(_commandBuffers[_frameCounter], 0);

    // Update Scene
    _scene->update(_frameCounter);

    // Record command buffer
    // Some scene objects may need to know the image index for rendering. for example when using multiple framebuffers.
    _scene->recordCommandBuffer(_commandBuffers[_frameCounter], imageIndex);

    // Submit the command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {_imageAvailableSemaphores[_imageCounter]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBuffers[_frameCounter];

    VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[_imageCounter]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(_ctx->graphicsQueue, 1, &submitInfo, _inFlightFences[_frameCounter]) != VK_SUCCESS) {
        spdlog::error("Failed to submit draw command buffer!");
        return;
    }

    // Present the image to the swap chain (after the command buffer is done)
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {_swapChain->getSwapChain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr; // Optional

    result = vkQueuePresentKHR(_ctx->presentQueue, &presentInfo);
    
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        invalidate();
    } else if (result != VK_SUCCESS) {
        spdlog::error("Failed to present swap chain image!");
    }

    _frameCounter = (_frameCounter + 1) % MAX_FRAMES_IN_FLIGHT;
    _imageCounter = (_imageCounter + 1) % _swapChain->getSwapChainImageCount();
}
