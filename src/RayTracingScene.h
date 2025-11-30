#pragma once 

#include "stdafx.h"
#include "VulkanContext.h"
#include "Scene.h"
#include "structure/Buffer.h"
#include "structure/TLAS.h"
#include "structure/BLAS.h"
#include "structure/StorageImage.h"
#include "RayTracingPipeline.h"
#include "DescriptorSet.h"


class RayTracingScene : public Scene
{
public:
    RayTracingScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);
    ~RayTracingScene();

    void update(uint32_t currentImage) override;
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t targetSwapImageIndex) override;

private:

    // void createBottomLevelAccelerationStructures();
    // void createTopLevelAccelerationStructure();

    // Physical Device Properties / Features
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR _rayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR _accelerationStructureFeatures{};

    // Device Mesh
    std::shared_ptr<DeviceMesh> _deviceMesh;

    // Acceleration Structures
    std::unique_ptr<BLAS> _blas;
    std::unique_ptr<TLAS> _tlas;

    // Storage Image
    std::unique_ptr<StorageImage> _storageImage;

    // Uniform Buffer
    struct UniformData {
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
	} _ubo;
    std::array<std::unique_ptr<Buffer>, MAX_FRAMES_IN_FLIGHT> _uniformBuffers;
    void createUniformBuffers();

    // Scene Descriptor Set
    std::array<std::unique_ptr<DescriptorSet>, MAX_FRAMES_IN_FLIGHT> _descriptorSets;
    void createDescriptorSets();

    // Ray Tracing Pipeline
    std::unique_ptr<RayTracingPipeline> _rayTracingPipeline;
    void createRayTracingPipeline();

    // SBT
    std::unique_ptr<Buffer> raygenShaderBindingTable;
    std::unique_ptr<Buffer> missShaderBindingTable;
    std::unique_ptr<Buffer> hitShaderBindingTable;
    void createShaderBindingTables();
};