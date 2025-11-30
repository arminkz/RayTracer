#pragma once

#include "stdafx.h"
#include "VulkanContext.h"
#include "VulkanHelper.h"

class StorageImage {
public:
    StorageImage(std::shared_ptr<VulkanContext> ctx, uint32_t width, uint32_t height, VkFormat format);
    ~StorageImage();

    void destroy();

    VkImage getImage() const { return _image; }
    VkImageView getImageView() const { return _imageView; }

    VkDescriptorImageInfo getDescriptorInfo() const;

private:
    std::shared_ptr<VulkanContext> _ctx;

    uint32_t _width;
    uint32_t _height;
    VkFormat _format;

    VkImage _image = VK_NULL_HANDLE;
    VkDeviceMemory _imageMemory = VK_NULL_HANDLE;
    VkImageView _imageView = VK_NULL_HANDLE;
};