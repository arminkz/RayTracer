#include "DeviceMesh.h"
#include "../VulkanHelper.h"

DeviceMesh::DeviceMesh(std::shared_ptr<VulkanContext> ctx, const HostMesh& mesh, const VkTransformMatrixKHR& transform)
    : _ctx(std::move(ctx))
{
    _indexCount = static_cast<uint32_t>(mesh.indices.size());
    createVertexBuffer(mesh);
    createIndexBuffer(mesh);
    createTransformBuffer(transform);
}

DeviceMesh::~DeviceMesh() {
    vkDestroyBuffer(_ctx->device, _vertexBuffer, nullptr);
    vkFreeMemory(_ctx->device, _vertexBufferMemory, nullptr);

    vkDestroyBuffer(_ctx->device, _indexBuffer, nullptr);
    vkFreeMemory(_ctx->device, _indexBufferMemory, nullptr);

    vkDestroyBuffer(_ctx->device, _transformBuffer, nullptr);
    vkFreeMemory(_ctx->device, _transformBufferMemory, nullptr);
}

void DeviceMesh::createVertexBuffer(const HostMesh& mesh)
{
    // Create Vertex Buffer
    VkDeviceSize bufferSize = sizeof(mesh.vertices[0]) * mesh.vertices.size(); // Size of the vertex buffer

    // Create staging buffer which is visible by both GPU and CPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanHelper::createBuffer(_ctx,
        bufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        false,
        stagingBuffer, stagingBufferMemory);

    void* data;
    // Map the buffer memory into CPU addressable space
    vkMapMemory(_ctx->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    // Copy the vertex data to the mapped memory
    memcpy(data, mesh.vertices.data(), (size_t)bufferSize);
    // Unmap the memory
    vkUnmapMemory(_ctx->device, stagingBufferMemory); 

    // Create the vertex buffer
    VulkanHelper::createBuffer(_ctx,
        bufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        _vertexBuffer, _vertexBufferMemory);

    // Copy the data from the staging buffer to the vertex buffer
    VulkanHelper::copyBuffer(_ctx, stagingBuffer, _vertexBuffer, bufferSize);

    // Cleanup staging buffer
    vkDestroyBuffer(_ctx->device, stagingBuffer, nullptr); // Destroy the staging buffer
    vkFreeMemory(_ctx->device, stagingBufferMemory, nullptr); // Free the staging buffer memory

    // Get the device address of the vertex buffer
    _vertexBufferDeviceAddress.deviceAddress = VulkanHelper::getBufferDeviceAddress(_ctx, _vertexBuffer);
}

void DeviceMesh::createIndexBuffer(const HostMesh& mesh)
{
    // Create Index Buffer
    VkDeviceSize bufferSize = sizeof(mesh.indices[0]) * mesh.indices.size(); // Size of the index buffer

    // Create staging buffer which is visible by both GPU and CPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanHelper::createBuffer(_ctx,
        bufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        false, 
        stagingBuffer, stagingBufferMemory);

    void* data;
    // Map the buffer memory into CPU addressable space
    vkMapMemory(_ctx->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    // Copy the index data to the mapped memory
    memcpy(data, mesh.indices.data(), (size_t)bufferSize);
    // Unmap the memory
    vkUnmapMemory(_ctx->device, stagingBufferMemory); 

    // Create the index buffer
    VulkanHelper::createBuffer(_ctx,
        bufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        _indexBuffer, _indexBufferMemory);

    // Copy the data from the staging buffer to the index buffer
    VulkanHelper::copyBuffer(_ctx, stagingBuffer, _indexBuffer, bufferSize);

    // Cleanup staging buffer
    vkDestroyBuffer(_ctx->device, stagingBuffer, nullptr); // Destroy the staging buffer
    vkFreeMemory(_ctx->device, stagingBufferMemory, nullptr); // Free the staging buffer memory

    // Get the device address of the index buffer
    _indexBufferDeviceAddress.deviceAddress = VulkanHelper::getBufferDeviceAddress(_ctx, _indexBuffer);
}

void DeviceMesh::createTransformBuffer(const VkTransformMatrixKHR& transform)
{
    VkDeviceSize bufferSize = sizeof(VkTransformMatrixKHR);

    // Create staging buffer which is visible by both GPU and CPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    VulkanHelper::createBuffer(_ctx,
        bufferSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        false, 
        stagingBuffer, stagingBufferMemory);

    void* data;
    // Map the buffer memory into CPU addressable space
    vkMapMemory(_ctx->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    // Copy the transform data to the mapped memory
    memcpy(data, &transform, (size_t)bufferSize);
    // Unmap the memory
    vkUnmapMemory(_ctx->device, stagingBufferMemory); 

    // Create the transform buffer
    VulkanHelper::createBuffer(_ctx,
        bufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        _transformBuffer, _transformBufferMemory);

    // Copy the data from the staging buffer to the transform buffer
    VulkanHelper::copyBuffer(_ctx, stagingBuffer, _transformBuffer, bufferSize);

    // Cleanup staging buffer
    vkDestroyBuffer(_ctx->device, stagingBuffer, nullptr); // Destroy the staging buffer
    vkFreeMemory(_ctx->device, stagingBufferMemory, nullptr); // Free the staging buffer memory

    // Get the device address of the transform buffer
    _transformBufferDeviceAddress.deviceAddress = VulkanHelper::getBufferDeviceAddress(_ctx, _transformBuffer);
}