#include "AccelerationStructure.h"

AccelerationStructure::AccelerationStructure(std::shared_ptr<VulkanContext> ctx, const DeviceMesh& dmesh)
    : _ctx(std::move(ctx))
{
    // Constructor implementation (if needed)

    // For each mesh or geometry, set up the acceleration structure geometry
    // Should this be done in DeviceMesh?
    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    accelerationStructureGeometry.geometry.triangles.vertexData = dmesh.getVertexBufferDeviceAddress();
    accelerationStructureGeometry.geometry.triangles.maxVertex = 2;
    accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
    accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    accelerationStructureGeometry.geometry.triangles.indexData = dmesh.getIndexBufferDeviceAddress();
    accelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = 0;
    accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = nullptr;
    accelerationStructureGeometry.geometry.triangles.transformData = dmesh.getTransformBufferDeviceAddress();

    // Get size info
    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
    accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;                   //BLAS
    accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelerationStructureBuildGeometryInfo.geometryCount = 1;   //Update here if you have more geometries
    accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    const uint32_t numTriangles = 1;
    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
    accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(
        _ctx->device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelerationStructureBuildGeometryInfo,
        &numTriangles,
        &accelerationStructureBuildSizesInfo);

    // Create acceleration structure buffer
    createAccelerationStructureBuffer(accelerationStructureBuildSizesInfo);

    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
	accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	accelerationStructureCreateInfo.buffer = bottomLevelAS.buffer;
	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	vkCreateAccelerationStructureKHR(_ctx->device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS.handle);

}

AccelerationStructure::~AccelerationStructure()
{
    // Destructor implementation (if needed)
}


void AccelerationStructure::createAccelerationStructureBuffer(VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
{
    // Create buffer for the acceleration structure
    VulkanHelper::createBuffer(_ctx,
        buildSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        true,
        _buffer, _bufferMemory);

    // Get the device address of the acceleration structure buffer
    _deviceAddress = VulkanHelper::getBufferDeviceAddress(_ctx, _buffer);
}


void AccelerationStructure::createScratchBuffer(VkDeviceSize size)
{
    // Create scratch buffer
    VulkanHelper::createBuffer(_ctx,
        size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // scratch buffer is used only by the GPU, so DEVICE_LOCAL is sufficient
        true,
        _scratchBuffer, _scratchBufferMemory);

    // Get the device address of the scratch buffer
    _scratchBufferDeviceAddress = VulkanHelper::getBufferDeviceAddress(_ctx, _scratchBuffer);
}


void AccelerationStructure::destroyScratchBuffer()
{
    if (_scratchBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(_ctx->device, _scratchBuffer, nullptr);
        _scratchBuffer = VK_NULL_HANDLE;
    }
    if (_scratchBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(_ctx->device, _scratchBufferMemory, nullptr);
        _scratchBufferMemory = VK_NULL_HANDLE;
    }
    _scratchBufferDeviceAddress = 0;
}