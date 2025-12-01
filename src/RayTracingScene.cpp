#include "RayTracingScene.h"
#include "VulkanHelper.h"
#include "geometry/HostMesh.h"
#include "geometry/MeshFactory.h"
#include "geometry/DeviceMesh.h"
#include "structure/Buffer.h"
#include "structure/BLAS.h"
#include "structure/TLAS.h"
#include "structure/StorageImage.h"
#include "DescriptorSet.h"
#include "AssetPath.h"
#include "VulkanRT.h"


RayTracingScene::RayTracingScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain)
    : Scene(std::move(ctx), std::move(swapChain))
{
    // Get ray tracing pipeline properties (we need this for SBT creation later)
    _rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProperties2{};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &_rayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(_ctx->physicalDevice, &deviceProperties2);

    // Get acceleration structure features
    _accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &_accelerationStructureFeatures;
    vkGetPhysicalDeviceFeatures2(_ctx->physicalDevice, &deviceFeatures2);


    // Create Host Mesh consisting of a single triangle
    // HostMesh hmesh;
    // hmesh.vertices = {
    //     {{ 0.0f, -0.5f, 0.0f }},
    //     {{ 0.5f,  0.5f, 0.0f }},
    //     {{-0.5f,  0.5f, 0.0f }}
    // };
    // hmesh.indices = { 0, 1, 2 };

    HostMesh hmesh = MeshFactory::createSphereMesh(0.5f, 32, 16);

    // Create identity transform
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    // Create Device Mesh
    _deviceMesh = std::make_shared<DeviceMesh>(_ctx, hmesh, transformMatrix);
    spdlog::info("Device mesh created.");

    // Create BLAS
    _blas = std::make_unique<BLAS>(_ctx);
    _blas->initialize(*_deviceMesh);
    spdlog::info("BLAS created.");

    // Create TLAS
    _tlas = std::make_unique<TLAS>(_ctx);
    _tlas->initialize(*_blas);
    spdlog::info("TLAS created.");

    // Create Storage Image
    // use same format as swap chain to avoid RGB/BGR mismatch
    // cant use SRGB format for storage image, so convert to UNORM
    _storageImage = std::make_unique<StorageImage>(_ctx,
        _swapChain->getSwapChainExtent().width,
        _swapChain->getSwapChainExtent().height,
        VulkanHelper::convertToUnormFormat(_swapChain->getSwapChainImageFormat()));
    spdlog::info("Storage image created.");


    // Create Uniform Buffers
    createUniformBuffers();
    spdlog::info("Uniform buffers created.");

    // Create Descriptor Sets
    createDescriptorSets();

    // Create Raytracing Pipeline
    createRayTracingPipeline();

    // Create Shader Binding Tables
    createShaderBindingTables();


}

RayTracingScene::~RayTracingScene()
{
    // Wait for any unfinished GPU tasks
    vkDeviceWaitIdle(_ctx->device);
}

void RayTracingScene::update(uint32_t currentImage) {
    // Update any scene-specific data here (e.g., camera, animations)
    Scene::update(currentImage);

    // Camera matrices
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 
        static_cast<float>(_swapChain->getSwapChainExtent().width) / static_cast<float>(_swapChain->getSwapChainExtent().height), 
        0.1f, 10.0f);

    // Update uniform buffer
    _ubo.viewInverse = glm::inverse(view);
    _ubo.projInverse = glm::inverse(proj);

    // Copy data to uniform buffer
    _uniformBuffers[currentImage]->copyData(&_ubo, sizeof(UniformData));
}

void RayTracingScene::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t targetSwapImageIndex) {

    // Begin Command buffer recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        spdlog::error("Failed to begin recording command buffer!");
        return;
    }

    // Setup the buffer regions pointing to the shaders in our shader binding tables
    const uint32_t handleSizeAligned = VulkanHelper::alignedSize(_rayTracingPipelineProperties.shaderGroupHandleSize, _rayTracingPipelineProperties.shaderGroupHandleAlignment);

    VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry{};
    raygenShaderSbtEntry.deviceAddress = raygenShaderBindingTable->getDeviceAddress();
    raygenShaderSbtEntry.stride = handleSizeAligned;
    raygenShaderSbtEntry.size = handleSizeAligned;

    VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
    missShaderSbtEntry.deviceAddress = missShaderBindingTable->getDeviceAddress();
    missShaderSbtEntry.stride = handleSizeAligned;
    missShaderSbtEntry.size = handleSizeAligned;

    VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
    hitShaderSbtEntry.deviceAddress = hitShaderBindingTable->getDeviceAddress();
    hitShaderSbtEntry.stride = handleSizeAligned;
    hitShaderSbtEntry.size = handleSizeAligned;

    VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{}; // Not used yet


    // Dispatch the ray tracing commands
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rayTracingPipeline->getPipeline());

    std::array<VkDescriptorSet, 1> descriptorSets = {
        _descriptorSets[_currentFrame]->getDescriptorSet()  // Per-frame descriptor set
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, 
        _rayTracingPipeline->getPipelineLayout(), 0, 1, 
        descriptorSets.data(), 0, nullptr);

    
    vkrt::vkCmdTraceRaysKHR(
        commandBuffer,
        &raygenShaderSbtEntry,
        &missShaderSbtEntry,
        &hitShaderSbtEntry,
        &callableShaderSbtEntry,
        _swapChain->getSwapChainExtent().width,
        _swapChain->getSwapChainExtent().height,
        1
    );

    // Make swap chain image ready for copy
    VulkanHelper::transitionImageLayout(_ctx,
        commandBuffer,
        _swapChain->getSwapChainImages()[targetSwapImageIndex],
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Make storage image ready for copy
    VulkanHelper::transitionImageLayout(_ctx, 
        commandBuffer,
        _storageImage->getImage(),
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);


    // Copy storage image to swap chain image
    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.srcOffset = { 0, 0, 0 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstOffset = { 0, 0, 0 };
    copyRegion.extent = { _swapChain->getSwapChainExtent().width, _swapChain->getSwapChainExtent().height, 1 };
    vkCmdCopyImage(
        commandBuffer,
        _storageImage->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        _swapChain->getSwapChainImages()[targetSwapImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion
    );


    // Transition swap chain image back for presentation
    VulkanHelper::transitionImageLayout(_ctx,
        commandBuffer,
        _swapChain->getSwapChainImages()[targetSwapImageIndex],
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Transition ray tracing output storage image back to general layout
    VulkanHelper::transitionImageLayout(_ctx,
        commandBuffer,
        _storageImage->getImage(),
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL);


    // End the command buffer recording
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        spdlog::error("Failed to record command buffer!");
        return;
    }
}

void RayTracingScene::createDescriptorSets() {

    // Create one descriptor set per frame in flight
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::vector<Descriptor> descriptors = {
            // Bare minimum required descriptors for ray tracing
            Descriptor(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1, _tlas->getDescriptorInfo()),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1, _storageImage->getDescriptorInfo()),
            Descriptor(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1, _uniformBuffers[i]->getDescriptorInfo()),

            // Mesh vertex and index buffers
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1, _deviceMesh->getVertexBufferDescriptorInfo()),
            Descriptor(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1, _deviceMesh->getIndexBufferDescriptorInfo())
        };
        _descriptorSets[i] = std::make_unique<DescriptorSet>(_ctx, descriptors);
    }

    spdlog::info("Descriptor sets created successfully.");
}

void RayTracingScene::createUniformBuffers() {

    // Create one uniform buffer per frame in flight
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _uniformBuffers[i] = std::make_unique<Buffer>(_ctx);
        _uniformBuffers[i]->initialize(sizeof(UniformData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            false
        );
    }
}

void RayTracingScene::createRayTracingPipeline() {

    VkDescriptorSetLayout sceneDSL = _descriptorSets[0]->getDescriptorSetLayout();

    RayTracingPipelineParams pipelineParams{};
    pipelineParams.descriptorSetLayouts = { sceneDSL };
    pipelineParams.name = "RayTracingPipeline";

    _rayTracingPipeline = std::make_unique<RayTracingPipeline>(_ctx, 
        AssetPath::getInstance()->get("spv/raygen_rgen.spv"),
        AssetPath::getInstance()->get("spv/miss_rmiss.spv"),
        AssetPath::getInstance()->get("spv/closesthit_rchit.spv"),
        pipelineParams);
}

void RayTracingScene::createShaderBindingTables() {
    const uint32_t handleSize = _rayTracingPipelineProperties.shaderGroupHandleSize;
    const uint32_t handleSizeAligned = VulkanHelper::alignedSize(handleSize, _rayTracingPipelineProperties.shaderGroupHandleAlignment);
    const uint32_t groupCount = _rayTracingPipeline->getShaderGroups().size();
    const uint32_t sbtSize = groupCount * handleSizeAligned;

    // Get shader group handles
    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    if (vkrt::vkGetRayTracingShaderGroupHandlesKHR(
            _ctx->device,
            _rayTracingPipeline->getPipeline(),
            0,
            groupCount,
            sbtSize,
            shaderHandleStorage.data()) != VK_SUCCESS) {
        spdlog::error("Failed to get ray tracing shader group handles!");
        throw std::runtime_error("Failed to get ray tracing shader group handles!");
    }

    // Create SBT buffers
    const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	const VkMemoryPropertyFlags memoryPropsFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    raygenShaderBindingTable = std::make_unique<Buffer>(_ctx);
    raygenShaderBindingTable->initialize(handleSize, bufferUsageFlags, memoryPropsFlags, true);

    missShaderBindingTable = std::make_unique<Buffer>(_ctx);
    missShaderBindingTable->initialize(handleSize, bufferUsageFlags, memoryPropsFlags, true);

    hitShaderBindingTable = std::make_unique<Buffer>(_ctx);
    hitShaderBindingTable->initialize(handleSize, bufferUsageFlags, memoryPropsFlags, true);


    // Copy shader group handles to SBT buffers
    raygenShaderBindingTable->copyData(shaderHandleStorage.data() + 0*handleSizeAligned, handleSize);
    missShaderBindingTable->copyData(shaderHandleStorage.data() + 1*handleSizeAligned, handleSize);
    hitShaderBindingTable->copyData(shaderHandleStorage.data() + 2*handleSizeAligned, handleSize);

    spdlog::info("Shader binding tables created successfully.");
}