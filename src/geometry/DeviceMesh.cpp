#include "DeviceMesh.h"
#include "../VulkanHelper.h"

DeviceMesh::DeviceMesh(std::shared_ptr<VulkanContext> ctx, const HostMesh& mesh, const VkTransformMatrixKHR& transform)
    : _ctx(ctx), _vertexCount(0), _indexCount(0)
{
    _vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    _indexCount = static_cast<uint32_t>(mesh.indices.size());
    createVertexBuffer(mesh);
    createIndexBuffer(mesh);
    createTransformBuffer(transform);
}

void DeviceMesh::createVertexBuffer(const HostMesh& mesh)
{
    VkDeviceSize bufferSize = sizeof(mesh.vertices[0]) * mesh.vertices.size();

    Buffer stagingBuffer(_ctx, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer.copyData(mesh.vertices.data(), bufferSize);

    _vertexBuffer = std::make_unique<Buffer>(_ctx, bufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true);

    VulkanHelper::copyBuffer(_ctx, stagingBuffer.getBuffer(), _vertexBuffer->getBuffer(), bufferSize);
}

void DeviceMesh::createIndexBuffer(const HostMesh& mesh)
{
    VkDeviceSize bufferSize = sizeof(mesh.indices[0]) * mesh.indices.size();

    Buffer stagingBuffer(_ctx, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer.copyData(mesh.indices.data(), bufferSize);

    _indexBuffer = std::make_unique<Buffer>(_ctx, bufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true);

    VulkanHelper::copyBuffer(_ctx, stagingBuffer.getBuffer(), _indexBuffer->getBuffer(), bufferSize);
}

void DeviceMesh::createTransformBuffer(const VkTransformMatrixKHR& transform)
{
    VkDeviceSize bufferSize = sizeof(VkTransformMatrixKHR);

    Buffer stagingBuffer(_ctx, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    stagingBuffer.copyData(&transform, bufferSize);

    _transformBuffer = std::make_unique<Buffer>(_ctx, bufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true);

    VulkanHelper::copyBuffer(_ctx, stagingBuffer.getBuffer(), _transformBuffer->getBuffer(), bufferSize);
}

VkDeviceOrHostAddressConstKHR DeviceMesh::getVertexBufferDeviceAddress() const
{
    VkDeviceOrHostAddressConstKHR addr{};
    addr.deviceAddress = _vertexBuffer->getDeviceAddress();
    return addr;
}

VkDeviceOrHostAddressConstKHR DeviceMesh::getIndexBufferDeviceAddress() const
{
    VkDeviceOrHostAddressConstKHR addr{};
    addr.deviceAddress = _indexBuffer->getDeviceAddress();
    return addr;
}

VkDeviceOrHostAddressConstKHR DeviceMesh::getTransformBufferDeviceAddress() const
{
    VkDeviceOrHostAddressConstKHR addr{};
    addr.deviceAddress = _transformBuffer->getDeviceAddress();
    return addr;
}

VkDescriptorBufferInfo DeviceMesh::getVertexBufferDescriptorInfo() const
{
    return _vertexBuffer->getDescriptorInfo();
}

VkDescriptorBufferInfo DeviceMesh::getIndexBufferDescriptorInfo() const
{
    return _indexBuffer->getDescriptorInfo();
}
