#pragma once
#include "stdafx.h"
#include "VulkanContext.h"


struct Descriptor
{
    uint32_t binding;
    VkDescriptorType type;
    VkShaderStageFlags stages;
    uint32_t count = 1;
    std::optional<VkDescriptorBufferInfo> bufferInfo;
    std::optional<VkDescriptorImageInfo> imageInfo;
    std::optional<VkWriteDescriptorSetAccelerationStructureKHR> accelStructInfo;

    // For buffer types (UBOs, SSBOs)
    Descriptor(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages, uint32_t count, const VkDescriptorBufferInfo& buffer)
        : binding(binding), type(type), stages(stages), count(count), bufferInfo(buffer) {}

    // For images (textures)
    Descriptor(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages, uint32_t count, const VkDescriptorImageInfo& image)
        : binding(binding), type(type), stages(stages), count(count), imageInfo(image) {}

    // For acceleration structures (TLAS)
    Descriptor(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages, uint32_t count, const VkWriteDescriptorSetAccelerationStructureKHR& accelStruct)
        : binding(binding), type(type), stages(stages), count(count), accelStructInfo(accelStruct) {}
};

class DescriptorSet
{
public:
    DescriptorSet(std::shared_ptr<VulkanContext> ctx, const std::vector<Descriptor>& descriptors);
    ~DescriptorSet();

    VkDescriptorSetLayout getDescriptorSetLayout() const { return _descriptorSetLayout; }
    VkDescriptorSet getDescriptorSet() const { return _descriptorSet; }

private:
    std::shared_ptr<VulkanContext> _ctx;

    VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet _descriptorSet = VK_NULL_HANDLE;

    void createDescriptorSetLayout(const std::vector<Descriptor>& descriptors);
    void createDescriptorSet(const std::vector<Descriptor>& descriptors);
};