#include "VulkanHelper.h"


namespace VulkanHelper {

    VkCommandBuffer beginSingleTimeCommands(const std::shared_ptr<VulkanContext>& ctx) {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ctx->commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Optional

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    void endSingleTimeCommands(const std::shared_ptr<VulkanContext>& ctx, VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(ctx->graphicsQueue);

        vkFreeCommandBuffers(ctx->device, ctx->commandPool, 1, &commandBuffer);
    }


    uint32_t findMemoryType(const std::shared_ptr<VulkanContext>& ctx, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(ctx->physicalDevice, &memProperties);
    
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        spdlog::error("Failed to find suitable memory type!");
        return -1;
    }


    void createBuffer(const std::shared_ptr<VulkanContext>& ctx, 
                      VkDeviceSize size, 
                      VkBufferUsageFlags usage, 
                      VkMemoryPropertyFlags properties,
                      bool needsDeviceAddress,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory) 
    {
        // Create buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Only one queue family will use this buffer
    
        if (vkCreateBuffer(ctx->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            spdlog::error("Failed to create buffer!");
            return;
        }
    
        // Get memory requirements for the buffer
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(ctx->device, buffer, &memRequirements);
    
        // Allocate memory for the buffer
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        if (needsDeviceAddress) {
            VkMemoryAllocateFlagsInfo allocFlagsInfo{};
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            allocInfo.pNext = &allocFlagsInfo;
        }

        allocInfo.memoryTypeIndex = findMemoryType(ctx, memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(ctx->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            spdlog::error("Failed to allocate buffer memory!");
            return;
        }
    
        // Bind the buffer memory to the buffer
        vkBindBufferMemory(ctx->device, buffer, bufferMemory, 0);
    }


    void copyBuffer(const std::shared_ptr<VulkanContext>& ctx, 
                    VkBuffer srcBuffer, 
                    VkBuffer dstBuffer, 
                    VkDeviceSize size) 
    {
        // Create a command buffer for the copy operation
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx);
    
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    
        // Submit the command buffer and wait for it to finish executing
        endSingleTimeCommands(ctx, commandBuffer);
    }


    void copyBufferToImage(const std::shared_ptr<VulkanContext>& ctx, 
                           VkBuffer buffer, 
                           VkImage image, 
                           uint32_t width, 
                           uint32_t height) 
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx);
        
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0; // Optional
        region.bufferImageHeight = 0; // Optional
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;
    
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(ctx, commandBuffer);
    }


    void copyBufferToCubemap(const std::shared_ptr<VulkanContext>& ctx, 
                             VkBuffer buffer, 
                             VkImage image, 
                             uint32_t width, 
                             uint32_t height) 
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx);
    
        std::vector<VkBufferImageCopy> regions(6);
        VkDeviceSize layerSize = width * height * 4; // assuming 4 bytes per pixel (RGBA8)

        for (uint32_t face = 0; face < 6; ++face) {
            VkBufferImageCopy region{};
            region.bufferOffset = layerSize * face;
            region.bufferRowLength = 0; // tightly packed
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = {0, 0, 0};
            region.imageExtent = {width, height, 1};

            regions[face] = region;
        }

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(regions.size()), regions.data());

        endSingleTimeCommands(ctx, commandBuffer);
    }


    void copyImageToBuffer(const std::shared_ptr<VulkanContext>& ctx, 
                           VkImage image, 
                           VkBuffer buffer, 
                           uint32_t width, 
                           uint32_t height) 
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx);
    
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0; // Optional
        region.bufferImageHeight = 0; // Optional
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;
    
        vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);
    
        endSingleTimeCommands(ctx, commandBuffer);
    }
    

    uint64_t getBufferDeviceAddress(const std::shared_ptr<VulkanContext>& ctx, VkBuffer buffer) 
    {
        VkBufferDeviceAddressInfo bufferDeviceAI{};
        bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferDeviceAI.buffer = buffer;
        return vkGetBufferDeviceAddress(ctx->device, &bufferDeviceAI);
    }


    void createImage(const std::shared_ptr<VulkanContext>& ctx, 
                     uint32_t width,
                     uint32_t height,
                     VkFormat format,
                     uint32_t mipLevels,
                     uint32_t arrayLayers,
                     VkSampleCountFlagBits numSamples, 
                     VkImageTiling tiling, 
                     VkImageUsageFlags usage, 
                     VkMemoryPropertyFlags properties, 
                     VkImage& image, VkDeviceMemory& imageMemory,
                     VkImageCreateFlags flags) 
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Initial layout
        imageInfo.usage = usage; // Usage flags
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Sharing mode
        imageInfo.samples = numSamples; // Number of samples for multisampling
        imageInfo.flags = flags;

        if (vkCreateImage(ctx->device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            spdlog::error("Failed to create image!");
            return;
        }
    
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(ctx->device, image, &memRequirements);
    
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(ctx, memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(ctx->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            spdlog::error("Failed to allocate image memory!");
            return;
        }
    
        vkBindImageMemory(ctx->device, image, imageMemory, 0);
    }


    VkImageView createImageView(const std::shared_ptr<VulkanContext>& ctx, 
                                VkImage image, 
                                VkFormat format, 
                                uint32_t mipLevels,
                                uint32_t arrayLayers,
                                VkImageAspectFlags aspectFlags,
                                VkImageViewType viewType)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = viewType;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = arrayLayers;

        VkImageView imageView;
        if (vkCreateImageView(ctx->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            spdlog::error("Failed to create image view!");
        }

        return imageView;
    }


    void transitionImageLayout(const std::shared_ptr<VulkanContext>& ctx, 
        VkCommandBuffer commandBuffer,
        VkImage image, 
        VkImageSubresourceRange subresourceRange, 
        VkImageLayout oldLayout, 
        VkImageLayout newLayout, 
        VkPipelineStageFlags srcStageMask, 
        VkPipelineStageFlags dstStageMask)
    {

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = subresourceRange;

        // Source access mask controls actions that have to be finished on the old layout
		// before it will be transitioned to the new layout
        switch(oldLayout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
            	// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
                barrier.srcAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
            	// Image is preinitialized
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes have been finished
                barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a depth/stencil attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source
                // Make sure any reads from the image have been finished
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination
                // Make sure any writes to the image have been finished
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is read by a shader
                // Make sure any shader reads from the image have been finished
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_GENERAL:
                // Image is used for general read/write access
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                break;

            default:
                spdlog::error("Unsupported old layout transition: {}", imageLayoutToString(oldLayout));
                break;            
        }

        // Destination access mask controls the dependency for the new layout
        switch(newLayout) {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination
                // Make sure any writes to the image have been finished
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source
                // Make sure any reads from the image have been finished
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment
                // Make sure any writes to the color buffer have been finished
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image will be used as a depth/stencil attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;;
                break;

            case VK_IMAGE_LAYOUT_GENERAL:
                // Image will be used for general read/write access
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                // Image will be used for presentation
                // Make sure any writes to the image have been finished
                if (barrier.srcAccessMask == 0)
                {
                    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                barrier.dstAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if (barrier.srcAccessMask == 0)
				{
					barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;

            default:
                spdlog::error("Unsupported new layout transition: {}", imageLayoutToString(newLayout));
                break;            
        }

        vkCmdPipelineBarrier(
            commandBuffer, 
            srcStageMask, 
            dstStageMask, 
            0, 
            0, nullptr, 
            0, nullptr, 
            1, &barrier);
    }


    void transitionImageLayout(const std::shared_ptr<VulkanContext>& ctx, 
        VkImage image, 
        VkImageSubresourceRange subresourceRange,
        VkImageLayout oldLayout, 
        VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx);
        transitionImageLayout(ctx, commandBuffer, image, subresourceRange, oldLayout, newLayout, srcStageMask, dstStageMask);
        endSingleTimeCommands(ctx, commandBuffer);
    }


    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    
        SwapChainSupportDetails details;
        
        // Capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);
    
        // Formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
        }
    
        // Present modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
        }
    
        return details;
    }


    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;
    
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    
        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
            }
            if (indices.isComplete()) {
                break;
            }
            i++;
        }
    
        return indices;
    }


    VkFormat findSupportedFormat(const std::shared_ptr<VulkanContext>& ctx, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(ctx->physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        spdlog::error("Failed to find supported format!");
        return VK_FORMAT_UNDEFINED;
    }


    VkFormat findDepthFormat(const std::shared_ptr<VulkanContext>& ctx) {
        return findSupportedFormat(ctx,
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }


    VkSampleCountFlagBits getMaxMsaaSampleCount(const std::shared_ptr<VulkanContext>& ctx) {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(ctx->physicalDevice, &physicalDeviceProperties);
    
        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
        if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
        if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
        if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
        if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
        if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
        return VK_SAMPLE_COUNT_1_BIT; // Fallback to 1 sample
    }
    

    bool hasStencilComponent(VkFormat format) {
        switch (format) {
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return true;
            default:
                return false;
        }
    }


    std::string formatToString(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
            case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
            case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
            case VK_FORMAT_D32_SFLOAT_S8_UINT: return "VK_FORMAT_D32_SFLOAT_S8_UINT";
            case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
            default: return "Unknown Format (" + std::to_string(format) + ")";
        }
    }


    std::string imageLayoutToString(VkImageLayout layout) {
        switch (layout) {
            case VK_IMAGE_LAYOUT_UNDEFINED: return "VK_IMAGE_LAYOUT_UNDEFINED";
            case VK_IMAGE_LAYOUT_GENERAL: return "VK_IMAGE_LAYOUT_GENERAL";
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL";
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL";
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL";
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL";
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR";
            default: return "Unknown Layout (" + std::to_string(layout) + ")";
        }
    }


    uint32_t alignedSize(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    VkFormat convertToUnormFormat(VkFormat format) {
        switch (format) {
            case VK_FORMAT_R8G8B8A8_SRGB:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case VK_FORMAT_B8G8R8A8_SRGB:
                return VK_FORMAT_B8G8R8A8_UNORM;
            default:
                return format; // Return the original format if no conversion is needed
        }
    }
}