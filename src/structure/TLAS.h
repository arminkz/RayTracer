#pragma once

#include "stdafx.h"
#include "VulkanContext.h"
#include "VulkanHelper.h"
#include "Buffer.h"


class TLAS {
public:
    TLAS(std::shared_ptr<VulkanContext> ctx);
    ~TLAS();

    void initialize(const std::vector<VkAccelerationStructureInstanceKHR>& instances);
    void update(const std::vector<VkAccelerationStructureInstanceKHR>& instances);

    VkWriteDescriptorSetAccelerationStructureKHR getDescriptorInfo() const;

private:
    std::shared_ptr<VulkanContext> _ctx;

    Buffer _asBuffer;
    VkAccelerationStructureKHR _handle = VK_NULL_HANDLE;
    uint64_t _deviceAddress = 0;
};