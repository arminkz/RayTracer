#pragma once
#include "../stdafx.h"
#include "../VulkanContext.h"
#include "HostMesh.h"

// Mesh representation on GPU
class DeviceMesh
{
public:
    DeviceMesh(std::shared_ptr<VulkanContext> ctx, const HostMesh& mesh, const VkTransformMatrixKHR& transform);
    ~DeviceMesh();

    uint32_t getVertexCount() const { return _vertexCount; }
    uint32_t getIndicesCount() const { return _indexCount; }

    VkBuffer getVertexBuffer() const { return _vertexBuffer; }
    VkDeviceOrHostAddressConstKHR getVertexBufferDeviceAddress() const { return _vertexBufferDeviceAddress; }
    
    VkBuffer getIndexBuffer() const { return _indexBuffer; }
    VkDeviceOrHostAddressConstKHR getIndexBufferDeviceAddress() const { return _indexBufferDeviceAddress; }
    
    VkBuffer getTransformBuffer() const { return _transformBuffer; }
    VkDeviceOrHostAddressConstKHR getTransformBufferDeviceAddress() const { return _transformBufferDeviceAddress; }

    VkDescriptorBufferInfo getVertexBufferDescriptorInfo() const;
    VkDescriptorBufferInfo getIndexBufferDescriptorInfo() const;

private:
    std::shared_ptr<VulkanContext> _ctx;

    //TODO: Use Buffer class instead of raw VkBuffer and VkDeviceMemory
    
    VkBuffer _vertexBuffer;
    VkDeviceMemory _vertexBufferMemory;
    VkDeviceOrHostAddressConstKHR _vertexBufferDeviceAddress{};
    uint32_t _vertexCount;

    VkBuffer _indexBuffer;
    VkDeviceMemory _indexBufferMemory;
    VkDeviceOrHostAddressConstKHR _indexBufferDeviceAddress{};
    uint32_t _indexCount;

    VkBuffer _transformBuffer;
    VkDeviceMemory _transformBufferMemory;
    VkDeviceOrHostAddressConstKHR _transformBufferDeviceAddress{};

    void createVertexBuffer(const HostMesh& mesh);
    void createIndexBuffer(const HostMesh& mesh);
    void createTransformBuffer(const VkTransformMatrixKHR& transform);
};