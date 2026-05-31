#pragma once

#include "pch.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/VulkanHelper.h"
#include "vulkan/resources/Buffer.h"


class TLAS {
public:
    TLAS(std::shared_ptr<VulkanContext> ctx, const std::vector<VkAccelerationStructureInstanceKHR>& instances);
    ~TLAS();

    TLAS(const TLAS&) = delete;
    TLAS& operator=(const TLAS&) = delete;
    TLAS(TLAS&&) = delete;
    TLAS& operator=(TLAS&&) = delete;

    void update(const std::vector<VkAccelerationStructureInstanceKHR>& instances);

    VkWriteDescriptorSetAccelerationStructureKHR getDescriptorInfo() const;

private:
    std::shared_ptr<VulkanContext> _ctx;

    std::unique_ptr<Buffer> _asBuffer;
    VkAccelerationStructureKHR _handle = VK_NULL_HANDLE;
    uint64_t _deviceAddress = 0;
};
