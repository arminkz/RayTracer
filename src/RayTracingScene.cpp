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
#include "InstanceData.h"


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


    // Create Storage Image at 2x resolution for supersampling
    // use same format as swap chain to avoid RGB/BGR mismatch
    // cant use SRGB format for storage image, so convert to UNORM
    _storageImage = std::make_unique<StorageImage>(_ctx,
        _swapChain->getSwapChainExtent().width * _supersampleScale,
        _swapChain->getSwapChainExtent().height * _supersampleScale,
        VulkanHelper::convertToUnormFormat(_swapChain->getSwapChainImageFormat()));
    spdlog::info("Storage image created at {}x resolution ({}x{}).",
        _supersampleScale,
        _swapChain->getSwapChainExtent().width * _supersampleScale,
        _swapChain->getSwapChainExtent().height * _supersampleScale);

    // Create Uniform Buffers
    createUniformBuffers();
    spdlog::info("Uniform buffers created.");

    // Create Geometry Templates
    createGeometryTemplates();

    // Create Scene Objects
    createSceneObjects();

    // Create Top Level Acceleration Structure
    createTLAS();

    // Create Instance Data Buffer
    createInstanceDataBuffer();

    // Create Descriptor Sets (needs TLAS and instance data buffer)
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

    // Advance time
    auto elapsedTime = std::chrono::high_resolution_clock::now() - _lastFrameTime;
    float elapsedSeconds = std::chrono::duration<float, std::chrono::seconds::period>(elapsedTime).count();
    if(!_isPaused) {
        _time += elapsedSeconds;
    }
    _lastFrameTime = std::chrono::high_resolution_clock::now();

    // Camera matrices
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 15.0f, 15.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 
        static_cast<float>(_swapChain->getSwapChainExtent().width) / static_cast<float>(_swapChain->getSwapChainExtent().height), 
        0.1f, 10.0f);
    proj[1][1] *= -1; // Invert Y for Vulkan

    // Rotate light around the scene
    float r = 10.0f;
    _ubo.lightPosition = glm::vec3(r * cos(_time*0.5), r, r * sin(_time*0.5));
    glm::vec3 lightDir = glm::normalize(-_ubo.lightPosition); // direction from point to light
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.999f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    _ubo.lightU = glm::normalize(glm::cross(up, lightDir));
    _ubo.lightV = glm::normalize(glm::cross(lightDir, _ubo.lightU));

    //Update light sphere position and TLAS
    _sceneObjects[_lightSphereIndex].transform = glm::translate(glm::mat4(1.0f), _ubo.lightPosition);
    _sceneObjects[_lightSphereIndex].transform = glm::scale(_sceneObjects[_lightSphereIndex].transform, glm::vec3(0.5f));
    updateTLAS();

    // Update uniform buffer
    _ubo.viewInverse = glm::inverse(view);
    _ubo.projInverse = glm::inverse(proj);
    _ubo.camPosition = glm::vec3(0.0f, 15.0f, 15.0f);
    
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
    const uint32_t missShaderCount = 2; // Primary miss and shadow miss

    VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry{};
    raygenShaderSbtEntry.deviceAddress = raygenShaderBindingTable->getDeviceAddress();
    raygenShaderSbtEntry.stride = handleSizeAligned;
    raygenShaderSbtEntry.size = handleSizeAligned;

    VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
    missShaderSbtEntry.deviceAddress = missShaderBindingTable->getDeviceAddress();
    missShaderSbtEntry.stride = handleSizeAligned;
    missShaderSbtEntry.size = handleSizeAligned * missShaderCount; // Size covers both miss shaders

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

    // Trace rays with supersampling
    vkrt::vkCmdTraceRaysKHR(
        commandBuffer,
        &raygenShaderSbtEntry,
        &missShaderSbtEntry,
        &hitShaderSbtEntry,
        &callableShaderSbtEntry,
        _swapChain->getSwapChainExtent().width * _supersampleScale,
        _swapChain->getSwapChainExtent().height * _supersampleScale,
        1
    );

    // Make swap chain image ready for copy
    VulkanHelper::transitionImageLayout(_ctx,
        commandBuffer,
        _swapChain->getSwapChainImages()[targetSwapImageIndex],
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Make storage image ready for blit (downsampling)
    VulkanHelper::transitionImageLayout(_ctx,
        commandBuffer,
        _storageImage->getImage(),
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);


    // Blit (downsample) 2x storage image to swapchain size with linear filtering
    VkImageBlit blitRegion{};
    blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blitRegion.srcOffsets[0] = { 0, 0, 0 };
    blitRegion.srcOffsets[1] = {
        static_cast<int32_t>(_swapChain->getSwapChainExtent().width * _supersampleScale),
        static_cast<int32_t>(_swapChain->getSwapChainExtent().height * _supersampleScale),
        1
    };
    blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blitRegion.dstOffsets[0] = { 0, 0, 0 };
    blitRegion.dstOffsets[1] = {
        static_cast<int32_t>(_swapChain->getSwapChainExtent().width),
        static_cast<int32_t>(_swapChain->getSwapChainExtent().height),
        1
    };

    vkCmdBlitImage(
        commandBuffer,
        _storageImage->getImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        _swapChain->getSwapChainImages()[targetSwapImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &blitRegion,
        VK_FILTER_LINEAR  // Linear filtering for smooth downsampling
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


void RayTracingScene::createGeometryTemplates() {
    // Identity transform for geometry templates (actual transforms are in TLAS instances)
    VkTransformMatrixKHR identityTransform = VulkanHelper::convertToVkTransform(glm::mat4(1.0f));

    // Plane (or large quad)
    HostMesh planeMesh = MeshFactory::createQuadMesh(1000.0f, 1000.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), true);
    _geometryTemplates["plane"].dmesh = std::make_unique<DeviceMesh>(_ctx, planeMesh, identityTransform);
    _geometryTemplates["plane"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["plane"].blas->initialize(*_geometryTemplates["plane"].dmesh);

    // Sphere
    HostMesh sphereMesh = MeshFactory::createSphereMesh(0.5f, 32, 16);
    _geometryTemplates["sphere"].dmesh = std::make_unique<DeviceMesh>(_ctx, sphereMesh, identityTransform);
    _geometryTemplates["sphere"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["sphere"].blas->initialize(*_geometryTemplates["sphere"].dmesh);

    // Box
    HostMesh boxMesh = MeshFactory::createBoxMesh(1.0f, 1.0f, 1.0f);
    _geometryTemplates["box"].dmesh = std::make_unique<DeviceMesh>(_ctx, boxMesh, identityTransform);
    _geometryTemplates["box"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["box"].blas->initialize(*_geometryTemplates["box"].dmesh);

    // Pyramid
    HostMesh pyramidMesh = MeshFactory::createPyramidMesh(1.0f, 1.0f, 1.0f);
    _geometryTemplates["pyramid"].dmesh = std::make_unique<DeviceMesh>(_ctx, pyramidMesh, identityTransform);
    _geometryTemplates["pyramid"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["pyramid"].blas->initialize(*_geometryTemplates["pyramid"].dmesh);

    // Doughnut
    HostMesh doughnutMesh = MeshFactory::createDoughnutMesh(0.35f, 0.5f, 32, 16);
    _geometryTemplates["doughnut"].dmesh = std::make_unique<DeviceMesh>(_ctx, doughnutMesh, identityTransform);
    _geometryTemplates["doughnut"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["doughnut"].blas->initialize(*_geometryTemplates["doughnut"].dmesh);

    // Cone
    HostMesh coneMesh = MeshFactory::createConeMesh(0.5f, 1.0f, 32, true);
    _geometryTemplates["cone"].dmesh = std::make_unique<DeviceMesh>(_ctx, coneMesh, identityTransform);
    _geometryTemplates["cone"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["cone"].blas->initialize(*_geometryTemplates["cone"].dmesh);

    // Cylinder
    HostMesh cylinderMesh = MeshFactory::createCylinderMesh(0.5f, 1.f, 32, true);
    _geometryTemplates["cylinder"].dmesh = std::make_unique<DeviceMesh>(_ctx, cylinderMesh, identityTransform);
    _geometryTemplates["cylinder"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["cylinder"].blas->initialize(*_geometryTemplates["cylinder"].dmesh);

    // Extruded Hexagon
    HostMesh extrudedHexagonMesh = MeshFactory::createPrismMesh(0.7f, 0.2f, 6, true);
    _geometryTemplates["extruded_hexagon"].dmesh = std::make_unique<DeviceMesh>(_ctx, extrudedHexagonMesh, identityTransform);
    _geometryTemplates["extruded_hexagon"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["extruded_hexagon"].blas->initialize(*_geometryTemplates["extruded_hexagon"].dmesh);

    // Icosahedron
    HostMesh icosahedronMesh = MeshFactory::createIcosahedronMesh(0.5f);
    _geometryTemplates["icosahedron"].dmesh = std::make_unique<DeviceMesh>(_ctx, icosahedronMesh, identityTransform);
    _geometryTemplates["icosahedron"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["icosahedron"].blas->initialize(*_geometryTemplates["icosahedron"].dmesh);

    // Rhombus
    HostMesh rhombusMesh = MeshFactory::createRhombusMesh(0.7f, 1.0f);
    _geometryTemplates["rhombus"].dmesh = std::make_unique<DeviceMesh>(_ctx, rhombusMesh, identityTransform);
    _geometryTemplates["rhombus"].blas = std::make_unique<BLAS>(_ctx);
    _geometryTemplates["rhombus"].blas->initialize(*_geometryTemplates["rhombus"].dmesh);


    // Put more geometry templates here as needed
}


void RayTracingScene::createSceneObjects() {
    // List of 10 colors for the spheres
    std::vector<glm::vec3> colors = {
        {1.0f, 0.0f, 0.0f}, // Red
        {0.0f, 1.0f, 0.0f}, // Green
        {0.0f, 0.0f, 1.0f}, // Blue
        {1.0f, 1.0f, 0.0f}, // Yellow
        {1.0f, 0.0f, 1.0f}, // Magenta
        {0.0f, 1.0f, 1.0f}, // Cyan
        {1.0f, 0.5f, 0.0f}, // Orange
        {0.5f, 0.0f, 1.0f}, // Purple
        {0.5f, 0.5f, 0.5f}, // Gray
        {1.0f, 1.0f, 1.0f}  // White
    };

    // Add a large plane to the scene
    {
        SceneObject obj;
        obj.geometryType = "plane";
        obj.transform = glm::mat4(1.0f); // Identity transform
        obj.materialType = 999;          // Checkerboard material
        obj.color = glm::vec3(0.8f, 0.8f, 0.8f); // Light gray
        _sceneObjects.push_back(obj);
    }

    // Add a light source visualization sphere
    {
        _lightSphereIndex = _sceneObjects.size(); // Store the index
        SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 20.0f, 0.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(0.5f)); // Small sphere
        obj.materialType = 1; // Emissive material
        obj.color = glm::vec3(1.0f, 1.0f, 0.8f); // Warm white light color
        _sceneObjects.push_back(obj);
    }

    //Objects placed in a grid fashion
    // Create a box
    {
        SceneObject obj;
        obj.geometryType = "box";
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 0.5f, 3.0f));
        obj.color = glm::vec3(0.7f, 0.3f, 0.2f); // Brownish
        _sceneObjects.push_back(obj);
    }

    // Create a Sphere
    {
        SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 0.6f, 0.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(1.2f)); // Scale up
        obj.color = glm::vec3(0.2f, 0.3f, 0.7f); // Bluish
        _sceneObjects.push_back(obj);
    }

    // Create a Pyramid
    {
        SceneObject obj;
        obj.geometryType = "pyramid";
        obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(-3.0f, 0.0f, -3.0f));
        obj.color = glm::vec3(0.3f, 0.7f, 0.2f); // Greenish
        _sceneObjects.push_back(obj);
    }

    // Create a Doughnut
    {
        SceneObject obj;
        obj.geometryType = "doughnut";
        obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
        obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.5f, 0.0f));
        obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        
        obj.color = glm::vec3(0.8f, 0.8f, 0.1f); // Yellowish
        _sceneObjects.push_back(obj);
    }

    // Create a Cone
    {
        SceneObject obj;
        obj.geometryType = "cone";
        obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.f, 0.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(1.2f)); // Scale up
        obj.color = glm::vec3(0.7f, 0.2f, 0.7f); // Purple-ish
        _sceneObjects.push_back(obj);
    }

    // Create a Cylinder
    {
        SceneObject obj;
        obj.geometryType = "cylinder";
        obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.5f, -3.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(1.0f, 1.0f, 1.0f)); // Scale up
        obj.color = glm::vec3(0.2f, 0.7f, 0.7f); // Teal-ish
        _sceneObjects.push_back(obj);
    }

    // Create a extruded hexagon
    {
        SceneObject obj;
        obj.geometryType = "extruded_hexagon";
        obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.6f, 3.0f));
        obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(1.f, 1.0f, 1.0f)); // Scale up
        obj.color = glm::vec3(0.9f, 0.6f, 0.2f); // Orange-ish
        _sceneObjects.push_back(obj);
    }

    // Create an icosahedron
    {
        SceneObject obj;
        obj.geometryType = "icosahedron";
        obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.5f, -3.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(1.2f)); // Scale up
        obj.color = glm::vec3(0.4f, 0.4f, 0.9f); // Light blue-ish
        _sceneObjects.push_back(obj);
    }

    // Create a rhombus
    {
        SceneObject obj;
        obj.geometryType = "rhombus";
        obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.5f, 3.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(1.0f, 1.5f, 1.0f)); // Scale up
        obj.color = glm::vec3(0.6f, 0.2f, 0.2f); // Reddish
        _sceneObjects.push_back(obj);
    }

    // 

}


void RayTracingScene::createTLAS() {

    _tlas = std::make_unique<TLAS>(_ctx);

    std::vector<VkAccelerationStructureInstanceKHR> instances;

    for (size_t i = 0; i < _sceneObjects.size(); i++) {
        const auto& obj = _sceneObjects[i];
        const auto& geom = _geometryTemplates[obj.geometryType];

        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = VulkanHelper::convertToVkTransform(obj.transform);
        instance.instanceCustomIndex = i; // Unique ID per object instance
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = geom.blas->getDeviceAddress();

        instances.push_back(instance);
    }

    _tlas->initialize(instances);
}

void RayTracingScene::updateTLAS() {
    std::vector<VkAccelerationStructureInstanceKHR> instances;

    for (size_t i = 0; i < _sceneObjects.size(); i++) {
        const auto& obj = _sceneObjects[i];
        const auto& geom = _geometryTemplates[obj.geometryType];

        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = VulkanHelper::convertToVkTransform(obj.transform);
        instance.instanceCustomIndex = i; // Unique ID per object instance
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = geom.blas->getDeviceAddress();

        instances.push_back(instance);
    }

    _tlas->update(instances);
}


void RayTracingScene::createInstanceDataBuffer() {
    // Create instance data array with material and buffer addresses
    std::vector<InstanceData> instanceDataArray;
    instanceDataArray.reserve(_sceneObjects.size());

    for (size_t i = 0; i < _sceneObjects.size(); i++) {
        const auto& obj = _sceneObjects[i];
        const auto& geom = _geometryTemplates[obj.geometryType];

        InstanceData instanceData{};
        instanceData.vertexBufferAddress = geom.dmesh->getVertexBufferDeviceAddress().deviceAddress;
        instanceData.indexBufferAddress = geom.dmesh->getIndexBufferDeviceAddress().deviceAddress;
        instanceData.materialType = obj.materialType;
        instanceData.color = obj.color;
        instanceData.metallic = 0.0f;  // Default material properties
        instanceData.roughness = 0.5f;
        instanceData._pad0 = 0.0f;
        instanceData._pad1 = 0.0f;

        instanceDataArray.push_back(instanceData);
    }

    // Create buffer to hold instance data
    const VkDeviceSize bufferSize = sizeof(InstanceData) * instanceDataArray.size();
    _instanceDataBuffer = std::make_unique<Buffer>(_ctx);
    _instanceDataBuffer->initialize(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        false
    );

    // Copy instance data to GPU
    _instanceDataBuffer->copyData(instanceDataArray.data(), bufferSize);

    spdlog::info("Instance data buffer created with {} instances.", instanceDataArray.size());
}


void RayTracingScene::createDescriptorSets() {

    // Create one descriptor set per frame in flight
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        std::vector<Descriptor> descriptors = {
            // Bare minimum required descriptors for ray tracing
            Descriptor(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1, _tlas->getDescriptorInfo()),
            Descriptor(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1, _storageImage->getDescriptorInfo()),
            Descriptor(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1, _uniformBuffers[i]->getDescriptorInfo()),

            // Instance data buffer (contains per-instance material and buffer addresses)
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 1, _instanceDataBuffer->getDescriptorInfo())
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

    // Setup miss shaders: primary miss (index 0) and shadow miss (index 1)
    std::vector<std::string> missShaderPaths = {
        AssetPath::getInstance()->get("spv/miss_rmiss.spv"),
        AssetPath::getInstance()->get("spv/shadow_rmiss.spv")
    };

    _rayTracingPipeline = std::make_unique<RayTracingPipeline>(_ctx,
        AssetPath::getInstance()->get("spv/raygen_rgen.spv"),
        missShaderPaths,
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

    // Raygen shader (group 0)
    raygenShaderBindingTable = std::make_unique<Buffer>(_ctx);
    raygenShaderBindingTable->initialize(handleSizeAligned, bufferUsageFlags, memoryPropsFlags, true);
    raygenShaderBindingTable->copyData(shaderHandleStorage.data() + 0*handleSizeAligned, handleSize);

    // Miss shaders (groups 1 and 2: primary miss and shadow miss)
    const uint32_t missShaderCount = 2;
    missShaderBindingTable = std::make_unique<Buffer>(_ctx);
    missShaderBindingTable->initialize(missShaderCount * handleSizeAligned, bufferUsageFlags, memoryPropsFlags, true);

    // Copy each miss shader handle individually with proper alignment
    for (uint32_t i = 0; i < missShaderCount; i++) {
        missShaderBindingTable->copyData(
            shaderHandleStorage.data() + (1 + i) * handleSizeAligned,
            handleSize,
            i * handleSizeAligned
        );
    }

    // Hit shader (group 3)
    hitShaderBindingTable = std::make_unique<Buffer>(_ctx);
    hitShaderBindingTable->initialize(handleSizeAligned, bufferUsageFlags, memoryPropsFlags, true);
    hitShaderBindingTable->copyData(shaderHandleStorage.data() + 3*handleSizeAligned, handleSize);

    spdlog::info("Shader binding tables created successfully 1 raygen, {} miss shaders, 1 hit shader", missShaderCount);
}