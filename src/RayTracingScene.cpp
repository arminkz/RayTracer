#include "RayTracingScene.h"
#include "VulkanHelper.h"
#include "gui/FontAwesome.h"
#include "structure/Buffer.h"
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

    // Create Scene Graph (geometry templates + scene objects)
    _sceneGraph = std::make_unique<SceneGraph>(_ctx);

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


void RayTracingScene::onSwapChainRecreated() {

    // Recreate storage image
    _storageImage = std::make_unique<StorageImage>(_ctx,
        _swapChain->getSwapChainExtent().width * _supersampleScale,
        _swapChain->getSwapChainExtent().height * _supersampleScale,
        VulkanHelper::convertToUnormFormat(_swapChain->getSwapChainImageFormat()));
    spdlog::info("Storage image recreated at {}x resolution ({}x{}).",
        _supersampleScale,
        _swapChain->getSwapChainExtent().width * _supersampleScale,
        _swapChain->getSwapChainExtent().height * _supersampleScale);

    // Recreate discriptor sets
    createDescriptorSets();
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
    

    // Light position
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


void RayTracingScene::recordToCommandBuffer(VkCommandBuffer commandBuffer, uint32_t targetSwapImageIndex) {

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

}


void RayTracingScene::createTLAS() {
    _tlas = std::make_unique<TLAS>(_ctx, _sceneGraph->buildInstanceList());
}

void RayTracingScene::updateTLAS() {
    _tlas->update(_sceneGraph->buildInstanceList());
}

void RayTracingScene::createInstanceDataBuffer() {
    auto instanceDataArray = _sceneGraph->buildInstanceDataArray();
    const VkDeviceSize bufferSize = sizeof(InstanceData) * instanceDataArray.size();
    _instanceDataBuffer = std::make_unique<Buffer>(_ctx,
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
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
        _uniformBuffers[i] = std::make_unique<Buffer>(_ctx,
            sizeof(UniformData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
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
    raygenShaderBindingTable = std::make_unique<Buffer>(_ctx, handleSizeAligned, bufferUsageFlags, memoryPropsFlags, true);
    raygenShaderBindingTable->copyData(shaderHandleStorage.data() + 0*handleSizeAligned, handleSize);

    // Miss shaders (groups 1 and 2: primary miss and shadow miss)
    const uint32_t missShaderCount = 2;
    missShaderBindingTable = std::make_unique<Buffer>(_ctx, missShaderCount * handleSizeAligned, bufferUsageFlags, memoryPropsFlags, true);

    // Copy each miss shader handle individually with proper alignment
    for (uint32_t i = 0; i < missShaderCount; i++) {
        missShaderBindingTable->copyData(
            shaderHandleStorage.data() + (1 + i) * handleSizeAligned,
            handleSize,
            i * handleSizeAligned
        );
    }

    // Hit shader (group 3)
    hitShaderBindingTable = std::make_unique<Buffer>(_ctx, handleSizeAligned, bufferUsageFlags, memoryPropsFlags, true);
    hitShaderBindingTable->copyData(shaderHandleStorage.data() + 3*handleSizeAligned, handleSize);

    spdlog::info("Shader binding tables created successfully 1 raygen, {} miss shaders, 1 hit shader", missShaderCount);
}



void RayTracingScene::buildUI() {
    ImGui::Begin("Scene Controls");

    ImGui::Checkbox(_isPaused ? ICON_FA_PLAY " Resume" : ICON_FA_PAUSE " Pause", &_isPaused);
    ImGui::Checkbox(ICON_FA_SYNC_ALT " Camera Orbit", &_cameraOrbiting);

    ImGui::Separator();

    if (ImGui::Button(ICON_FA_SAVE " Save State")) saveSceneState("scene_state.txt");
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load State")) loadSceneState("scene_state.txt");

    ImGui::End();
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