#include "Buffer.h"
#include "VulkanHelper.h"

Buffer::Buffer(std::shared_ptr<VulkanContext> ctx)
    : _ctx(std::move(ctx)), _mappedMemory(nullptr)
{
}

Buffer::~Buffer()
{
    spdlog::debug("Buffer destructor called");
    destroy();
}

void Buffer::initialize(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, bool needsDeviceAddress)
{
    // Create the buffer and allocate memory for it
    VulkanHelper::createBuffer(_ctx, size, usage, properties, needsDeviceAddress, _buffer, _bufferMemory);

    // Map the memory to the buffer
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        vkMapMemory(_ctx->device, _bufferMemory, 0, size, 0, &_mappedMemory);
    }

    // Get the device address if needed
    if (needsDeviceAddress) {
        _deviceAddress = VulkanHelper::getBufferDeviceAddress(_ctx, _buffer);
    }
}

void Buffer::destroy()
{
    // Unmap the memory if it was mapped
    if (_mappedMemory) {
        vkUnmapMemory(_ctx->device, _bufferMemory);
        _mappedMemory = nullptr;
    }

    // Destroy the buffer and free the memory
    if (_buffer) {
        vkDestroyBuffer(_ctx->device, _buffer, nullptr);
        vkFreeMemory(_ctx->device, _bufferMemory, nullptr);
        _buffer = VK_NULL_HANDLE;
        _bufferMemory = VK_NULL_HANDLE;
    }

    _deviceAddress = 0;
}

void Buffer::copyData(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (_mappedMemory) {
        memcpy(static_cast<int32_t*>(_mappedMemory) + offset, data, size);
    } else {
        throw std::runtime_error("Buffer is not mapped!");
    }
}

VkDescriptorBufferInfo Buffer::getDescriptorInfo() const {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = _buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    return bufferInfo;
}