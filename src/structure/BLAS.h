#pragma once

#include "stdafx.h"
#include "VulkanContext.h"
#include "VulkanHelper.h"
#include "Buffer.h"
#include "geometry/DeviceMesh.h"


class BLAS {
public:
    BLAS(std::shared_ptr<VulkanContext> ctx);
    ~BLAS();

    void initialize(const DeviceMesh& dmesh);
    uint64_t getDeviceAddress() const { return _deviceAddress; }

private:
    std::shared_ptr<VulkanContext> _ctx;

    Buffer _asBuffer;
    VkAccelerationStructureKHR _handle = VK_NULL_HANDLE;
    uint64_t _deviceAddress = 0;
    
};