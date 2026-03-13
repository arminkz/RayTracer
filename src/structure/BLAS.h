#pragma once

#include "stdafx.h"
#include "VulkanContext.h"
#include "VulkanHelper.h"
#include "Buffer.h"
#include "geometry/DeviceMesh.h"


class BLAS {
public:
    BLAS(std::shared_ptr<VulkanContext> ctx, const DeviceMesh& dmesh);
    ~BLAS();

    uint64_t getDeviceAddress() const { return _deviceAddress; }

private:
    std::shared_ptr<VulkanContext> _ctx;

    std::unique_ptr<Buffer> _asBuffer;
    VkAccelerationStructureKHR _handle = VK_NULL_HANDLE;
    uint64_t _deviceAddress = 0;
    
};