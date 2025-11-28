#pragma once

#include "stdafx.h"
#include "VulkanContext.h"
#include "VulkanHelper.h"
#include "geometry/DeviceMesh.h"


class AccelerationStructure {
public:
    AccelerationStructure(std::shared_ptr<VulkanContext> ctx, const DeviceMesh& dmesh);
    ~AccelerationStructure();


private:
    std::shared_ptr<VulkanContext> _ctx;

    void createAccelerationStructureBuffer(VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);

    VkBuffer _buffer = VK_NULL_HANDLE;
    VkDeviceMemory _bufferMemory = VK_NULL_HANDLE;
    uint64_t _deviceAddress = 0;

    // Scratch buffer for building acceleration structures
    void createScratchBuffer(VkDeviceSize size);
    void destroyScratchBuffer();

    VkBuffer _scratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory _scratchBufferMemory = VK_NULL_HANDLE;
    uint64_t _scratchBufferDeviceAddress = 0;
};