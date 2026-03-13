#pragma once
#include "../stdafx.h"
#include "../VulkanContext.h"
#include "../structure/Buffer.h"
#include "HostMesh.h"

// Mesh representation on GPU
class DeviceMesh
{
public:
    DeviceMesh(std::shared_ptr<VulkanContext> ctx, const HostMesh& mesh, const VkTransformMatrixKHR& transform);
    ~DeviceMesh() = default;

    uint32_t getVertexCount() const { return _vertexCount; }
    uint32_t getIndicesCount() const { return _indexCount; }

    VkBuffer getVertexBuffer() const { return _vertexBuffer->getBuffer(); }
    VkDeviceOrHostAddressConstKHR getVertexBufferDeviceAddress() const;

    VkBuffer getIndexBuffer() const { return _indexBuffer->getBuffer(); }
    VkDeviceOrHostAddressConstKHR getIndexBufferDeviceAddress() const;

    VkBuffer getTransformBuffer() const { return _transformBuffer->getBuffer(); }
    VkDeviceOrHostAddressConstKHR getTransformBufferDeviceAddress() const;

    VkDescriptorBufferInfo getVertexBufferDescriptorInfo() const;
    VkDescriptorBufferInfo getIndexBufferDescriptorInfo() const;

private:
    std::shared_ptr<VulkanContext> _ctx;

    std::unique_ptr<Buffer> _vertexBuffer;
    uint32_t _vertexCount;

    std::unique_ptr<Buffer> _indexBuffer;
    uint32_t _indexCount;

    std::unique_ptr<Buffer> _transformBuffer;

    void createVertexBuffer(const HostMesh& mesh);
    void createIndexBuffer(const HostMesh& mesh);
    void createTransformBuffer(const VkTransformMatrixKHR& transform);
};
