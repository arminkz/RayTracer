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

    // Create Camera
    TurnTableCameraParams cameraParams;
    cameraParams.initialElevation = -0.6f;
    _camera = std::make_unique<TurnTableCamera>(cameraParams);

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

    // Camera orbiting
    if(_cameraOrbiting) {
        _camera->rotateHorizontally(0.01f);
    }

    // Camera matrices
    glm::mat4 view = _camera->getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 
        static_cast<float>(_swapChain->getSwapChainExtent().width) / static_cast<float>(_swapChain->getSwapChainExtent().height), 
        0.1f, 10.0f);
    proj[1][1] *= -1; // Invert Y for Vulkan
    

    // Rotate light around the scene
    float r = 25.0f;
    _ubo.lightPosition = glm::vec3(r * glm::radians(60.f), 1.3f * r, r * sin(glm::radians(60.f)));
    glm::vec3 lightDir = glm::normalize(-_ubo.lightPosition); // direction from point to light
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.999f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    _ubo.lightU = glm::normalize(glm::cross(up, lightDir));
    _ubo.lightV = glm::normalize(glm::cross(lightDir, _ubo.lightU));

    //Update light sphere position and TLAS
    // _sceneObjects[_lightSphereIndex].transform = glm::translate(glm::mat4(1.0f), _ubo.lightPosition);
    // _sceneObjects[_lightSphereIndex].transform = glm::scale(_sceneObjects[_lightSphereIndex].transform, glm::vec3(0.5f));
    updateTLAS();

    // Update uniform buffer
    _ubo.viewInverse = glm::inverse(view);
    _ubo.projInverse = glm::inverse(proj);
    _ubo.camPosition = _camera->getPosition();
    
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
    HostMesh sphereMesh = MeshFactory::createSphereMesh(0.5f, 64, 32);
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
    HostMesh doughnutMesh = MeshFactory::createDoughnutMesh(0.35f, 0.5f, 64, 32);
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
        obj.metallic = 0.0;
        obj.roughness = 0.8;
        obj.transparency = 0.0;
        _sceneObjects.push_back(obj);
    }

    // Add a light source visualization sphere
    // {
    //     _lightSphereIndex = _sceneObjects.size(); // Store the index
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 20.0f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(0.5f)); // Small sphere
    //     obj.materialType = 1; // Emissive material
    //     obj.color = glm::vec3(1.0f, 1.0f, 0.8f); // Warm white light color
    //     _sceneObjects.push_back(obj);
    // }

    //Objects placed in a grid fashion
    // Create a box
    // {
    //     SceneObject obj;
    //     obj.geometryType = "box";
    //     obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, 0.5f, 3.0f));
    //     obj.color = glm::vec3(0.7f, 0.3f, 0.2f); // Brownish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     _sceneObjects.push_back(obj);
    // }

    // Create a reflective metallic sphere (center)
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 2.5f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(5.0f)); // Scale up
    //     obj.color = glm::vec3(1.0f, 0.766f, 0.336f); // Gold
    //     obj.metallic = 1.0;
    //     obj.roughness = 0.2;
    //     obj.transparency = 0.0;  // Opaque
    //     _sceneObjects.push_back(obj);
    // }


    // Plastic Cylinder
    {
        SceneObject obj;
        obj.geometryType = "sphere";
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.01f, 0.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(2.0f)); // Scale up
        obj.color = glm::vec3(0.1f, 0.2f, 0.9f); // Blue
        obj.metallic = 1.0;
        obj.roughness = 0.5;
        obj.transparency = 0.0;  // Opaque
        _sceneObjects.push_back(obj);
    }

    // Create a glass sphere (transparent with refraction)
    {
        SceneObject obj;
        obj.geometryType = "box";
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 2.05f, 0.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(.2f,4.0f,7.0f)); // Scale up
        obj.color = glm::vec3(0.95f, 0.98f, 1.0f); // Slight blue tint for glass
        obj.metallic = 0.0;
        obj.roughness = 0.05;
        obj.transparency = 1.00;  // Nearly fully transparent
        obj.ior = 1.52f;  // Glass index of refraction
        obj.absorbance = glm::vec3(0.1f,0.1f,0.1f);
        _sceneObjects.push_back(obj);
    }


    {
        SceneObject obj;
        obj.geometryType = "doughnut";
        obj.transform = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 1.01f, 3.0f));
        obj.transform = glm::scale(obj.transform, glm::vec3(2.f)); // Scale up
        obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        obj.color = glm::vec3(0.05f, 0.7f, 0.01f); // Slight blue tint for glass
        obj.metallic = 0.0;
        obj.roughness = 0.8;
        obj.transparency = 0.8;  // Nearly fully transparent
        obj.absorbance = glm::vec3(4.f, 0.1f , 4.f);
        obj.ior = 1.02;
        _sceneObjects.push_back(obj);
    }

    // // Create more spheres in random positions without overlapping
    // std::vector<glm::vec3> spherePositions; // Track placed sphere positions
    // const float sphereRadius = 0.2f; // Base sphere radius
    // const float minDistance = sphereRadius * 2.0f + 0.5f; // Minimum distance between centers (with spacing)
    // const int numSpheres = 30;
    // const int maxAttempts = 100; // Max attempts per sphere to find valid position

    // for(int i = 0; i < numSpheres; i++) {
    //     bool validPosition = false;
    //     glm::vec3 position;
    //     int attempts = 0;

    //     while(!validPosition && attempts < maxAttempts) {
    //         // Generate random position in a circular area
    //         float angle = static_cast<float>(rand()) / RAND_MAX * 360.0f;
    //         float radius = 3.0f + static_cast<float>(rand()) / RAND_MAX * 12.0f; // Random radius 3-15
    //         float x = radius * cos(glm::radians(angle));
    //         float z = radius * sin(glm::radians(angle));
    //         position = glm::vec3(x, 0.5f, z);

    //         // Check if this position overlaps with any existing sphere
    //         validPosition = true;
    //         for(const auto& existingPos : spherePositions) {
    //             float dist = glm::distance(glm::vec2(position.x, position.z),
    //                                       glm::vec2(existingPos.x, existingPos.z));
    //             if(dist < minDistance) {
    //                 validPosition = false;
    //                 break;
    //             }
    //         }
    //         attempts++;
    //     }

    //     // Only add sphere if we found a valid position
    //     if(validPosition) {
    //         spherePositions.push_back(position);

    //         SceneObject obj;
    //         obj.geometryType = i % 3 == 0 ? "sphere" : "icosahedron";
    //         obj.transform = glm::translate(glm::mat4(1.0f), position);
    //         obj.transform = glm::scale(obj.transform, glm::vec3(1.0f));
    //         obj.color = colors[i % colors.size()];
    //         obj.metallic = i % 2 == 0 ? 0.0 : 1.0;
    //         obj.roughness = 0.2;
    //         obj.transparency = i % 4 == 0 ? 1.0 : 0.0;
    //         _sceneObjects.push_back(obj);
    //     }
    // }

    // Create a Pyramid
    // {
    //     SceneObject obj;
    //     obj.geometryType = "pyramid";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(-3.0f, 0.0f, -3.0f));
    //     obj.color = glm::vec3(0.3f, 0.7f, 0.2f); // Greenish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     _sceneObjects.push_back(obj);
    // }

    // Semi-Transparent (jello like)
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
    //     obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.502f, 0.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.color = glm::vec3(1.0f, 0.766f, 0.336f); // Yellowish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.02;
    //     obj.transparency = 0.99; // we still want shadows
    //     obj.ior = 1.00029;
    //     obj.absorbance = glm::vec3(8.f, 8.0f, 2.f); 
    //     _sceneObjects.push_back(obj);
    // }

    // Silver
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
    //     obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.502f, 0.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.color = glm::vec3(0.97, 0.96, 0.91);
    //     obj.metallic = 1.0;
    //     obj.roughness = 0.1;
    //     obj.transparency = 0.0;
    //     _sceneObjects.push_back(obj);
    // }

    // Glass
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
    //     obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.502f, 0.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.color = glm::vec3(0.97, 0.96, 0.91);
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.1;
    //     obj.transparency = 1.0;
    //     obj.ior = 1.03;
    //     _sceneObjects.push_back(obj);
    // }

    // Plastic
    // {
    //     SceneObject obj;
    //     obj.geometryType = "sphere";
    //     obj.transform = glm::scale(glm::mat4(1.f), glm::vec3(1.5f)); // Scale up
    //     obj.transform = glm::translate(obj.transform, glm::vec3(0.0f, 0.502f, 0.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.color = glm::vec3(1.0, 0.8, 0.0);
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.9;
    //     obj.transparency = 0.99;
    //     obj.absorbance = glm::vec3(0.1,0.5,8.0);
    //     obj.ior = 1.001;
    //     _sceneObjects.push_back(obj);
    // }

    // Create a Cone
    // {
    //     SceneObject obj;
    //     obj.geometryType = "cone";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.2f)); // Scale up
    //     obj.color = glm::vec3(0.7f, 0.2f, 0.7f); // Purple-ish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     _sceneObjects.push_back(obj);
    // }

    // Create a Cylinder
    // {
    //     SceneObject obj;
    //     obj.geometryType = "cylinder";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.5f, -3.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.0f, 1.0f, 1.0f)); // Scale up
    //     obj.color = glm::vec3(0.2f, 0.7f, 0.7f); // Teal-ish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     _sceneObjects.push_back(obj);
    // }

    // Create a extruded hexagon
    // {
    //     SceneObject obj;
    //     obj.geometryType = "extruded_hexagon";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.6f, 3.0f));
    //     obj.transform = glm::rotate(obj.transform, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.f, 1.0f, 1.0f)); // Scale up
    //     obj.color = glm::vec3(0.9f, 0.6f, 0.2f); // Orange-ish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     obj.transparency = 1.0;
    //     _sceneObjects.push_back(obj);
    // }

    // Create an icosahedron
    // {
    //     SceneObject obj;
    //     obj.geometryType = "icosahedron";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.5f, -3.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.2f)); // Scale up
    //     obj.color = glm::vec3(0.4f, 0.4f, 0.9f); // Light blue-ish
    //     obj.metallic = 0.0;
    //     obj.roughness = 0.8;
    //     obj.transparency = 1.0;
    //     _sceneObjects.push_back(obj);
    // }

    // // Create a rhombus
    // {
    //     SceneObject obj;
    //     obj.geometryType = "rhombus";
    //     obj.transform = glm::translate(glm::mat4(1.f), glm::vec3(3.0f, 0.75f, 3.0f));
    //     obj.transform = glm::scale(obj.transform, glm::vec3(1.0f, 1.5f, 1.0f)); // Scale up
    //     obj.color = glm::vec3(0.6f, 0.2f, 0.2f); // Reddish
    //     obj.roughness = 0.8;
    //     obj.transparency = 1.0;
    //     _sceneObjects.push_back(obj);
    // }

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
        instanceData.metallic = obj.metallic;
        instanceData.roughness = 0.5f;
        instanceData.transparency = obj.transparency;
        instanceData.ior = obj.ior;
        instanceData.absorbance = obj.absorbance;

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
            Descriptor(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 1, _instanceDataBuffer->getDescriptorInfo())
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
        AssetPath::getInstance()->get("spv/shadow_rahit.spv"),
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

void RayTracingScene::handleMouseClick(float mx, float my) {
    return;
}

void RayTracingScene::handleMouseDrag(float dx, float dy) {
    // Update camera based on mouse drag
    _camera->rotateHorizontally(static_cast<float>(-dx) * 0.005f);
    _camera->rotateVertically(static_cast<float>(-dy) * 0.005f);
}

void RayTracingScene::handleMouseWheel(float dy) {
    // Zoom camera based on scroll
    _camera->changeZoom(static_cast<float>(dy) * 0.3f);
}

void RayTracingScene::handleKeyDown(int key, int scancode, int mods) {

    switch (key)
    {
    case SDLK_S:
        // Save scene state
        saveSceneState("scene_state.txt");
        spdlog::info("Scene state saved.");
        break;

    case SDLK_L:
        // Load scene state
        loadSceneState("scene_state.txt");
        spdlog::info("Scene state loaded.");
        break;

    case SDLK_O:
        _cameraOrbiting = !_cameraOrbiting;
        break;
    
    default:
        break;
    }
}

void RayTracingScene::saveSceneState(const std::string& filename)
{
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        spdlog::error("Failed to open file for saving scene state: {}", filename);
        return;
    }

    // Save scene time
    outFile << _time << std::endl;

    // Save camera state
    // Radius
    outFile << _camera->getRadius() << std::endl;
    // Elevation
    outFile << _camera->getElevation() << std::endl;
    // Azimuth
    outFile << _camera->getAzimuth() << std::endl;

    outFile.close();
}


void RayTracingScene::loadSceneState(const std::string& filename)
{
    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        spdlog::error("Failed to open file for loading scene state: {}", filename);
        return;
    }

    std::string line;

    // Load scene time
    std::getline(inFile, line);
    _time = std::stof(line);

    // Load camera state

    // Radius
    std::getline(inFile, line);
    float radius = std::stof(line);
    _camera->setRadius(radius);
    // Elevation
    std::getline(inFile, line);
    float elevation = std::stof(line);
    _camera->setElevation(elevation);
    // Azimuth
    std::getline(inFile, line);
    float azimuth = std::stof(line);
    _camera->setAzimuth(azimuth);

    inFile.close();
}