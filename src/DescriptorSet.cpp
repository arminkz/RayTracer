#include "DescriptorSet.h"


DescriptorSet::DescriptorSet(std::shared_ptr<VulkanContext> ctx, const std::vector<Descriptor>& descriptors)
    : _ctx(std::move(ctx))
{
    createDescriptorSetLayout(descriptors);
    createDescriptorSet(descriptors);
}


DescriptorSet::~DescriptorSet()
{
    vkDestroyDescriptorSetLayout(_ctx->device, _descriptorSetLayout, nullptr);
}


void DescriptorSet::createDescriptorSetLayout(const std::vector<Descriptor>& descriptors)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings(descriptors.size());

    for (size_t i = 0; i < descriptors.size(); ++i) {
        bindings[i].binding = descriptors[i].binding;
        bindings[i].descriptorType = descriptors[i].type;
        bindings[i].descriptorCount = descriptors[i].count;
        bindings[i].stageFlags = descriptors[i].stages;
        bindings[i].pImmutableSamplers = nullptr; // Optional
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(_ctx->device, &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}


void DescriptorSet::createDescriptorSet(const std::vector<Descriptor>& descriptors)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _ctx->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &_descriptorSetLayout;

    if (vkAllocateDescriptorSets(_ctx->device, &allocInfo, &_descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set!");
    }
    std::vector<VkWriteDescriptorSet> descriptorWrites;
    descriptorWrites.reserve(descriptors.size());

    for (const auto& d : descriptors) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = _descriptorSet;
        write.dstBinding = d.binding;
        write.dstArrayElement = 0;
        write.descriptorType = d.type;
        write.descriptorCount = d.count;

        if (d.bufferInfo.has_value()) {
            write.pBufferInfo = &d.bufferInfo.value();
        } else if (d.imageInfo.has_value()) {
            write.pImageInfo = &d.imageInfo.value();
        } else if (d.accelStructInfo.has_value()) {
            write.pNext = &d.accelStructInfo.value();
        }

        descriptorWrites.push_back(write);
    }

    vkUpdateDescriptorSets(_ctx->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}