#include "StorageImage.h"


StorageImage::StorageImage(std::shared_ptr<VulkanContext> ctx, uint32_t width, uint32_t height, VkFormat format)
    : _ctx(std::move(ctx)), _width(width), _height(height), _format(format)
{
    VulkanHelper::createImage(_ctx, _width, _height, _format, 1, 1, VK_SAMPLE_COUNT_1_BIT,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              _image, _imageMemory);

    _imageView = VulkanHelper::createImageView(_ctx, _image, _format, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);

    // Transition image layout to GENERAL for storage image usage
    VulkanHelper::transitionImageLayout(_ctx, _image,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL);
}

StorageImage::~StorageImage()
{
    destroy();
}

void StorageImage::destroy()
{
    vkDestroyImageView(_ctx->device, _imageView, nullptr);
    vkDestroyImage(_ctx->device, _image, nullptr);
    vkFreeMemory(_ctx->device, _imageMemory, nullptr);
}

VkDescriptorImageInfo StorageImage::getDescriptorInfo() const {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = _imageView;
    imageInfo.sampler = VK_NULL_HANDLE; // Not needed for storage images
    return imageInfo;
}