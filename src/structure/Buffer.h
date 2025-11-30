#pragma once

#include "stdafx.h"
#include "VulkanContext.h"

class Buffer
{
public:
    Buffer(std::shared_ptr<VulkanContext> ctx);
    ~Buffer();

    void initialize(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, bool needsDeviceAddress = false);
    void destroy();

    VkBuffer getBuffer() const { return _buffer; }
    VkDeviceMemory getMemory() const { return _bufferMemory; }
    void* getMappedMemory() const { return _mappedMemory; }
    uint64_t getDeviceAddress() const { return _deviceAddress; }

    void copyData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    VkDescriptorBufferInfo getDescriptorInfo() const;
    
private:
    std::shared_ptr<VulkanContext> _ctx;

    VkBuffer _buffer = VK_NULL_HANDLE;
    VkDeviceMemory _bufferMemory = VK_NULL_HANDLE;
    void* _mappedMemory = nullptr;
    uint64_t _deviceAddress = 0;
};