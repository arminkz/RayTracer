#pragma once 

#include "stdafx.h"
#include "VulkanContext.h"
#include "Scene.h"
#include "structure/Buffer.h"
#include "structure/TLAS.h"
#include "structure/StorageImage.h"
#include "SceneGraph.h"
#include "RayTracingPipeline.h"
#include "DescriptorSet.h"
#include "TurnTableCamera.h"


class RayTracingScene : public Scene
{
public:
    RayTracingScene(std::shared_ptr<VulkanContext> ctx, std::shared_ptr<SwapChain> swapChain);
    ~RayTracingScene();

    void update(uint32_t currentImage) override;
    void recordToCommandBuffer(VkCommandBuffer commandBuffer, uint32_t targetSwapImageIndex) override;
    void onSwapChainRecreated() override;

    void handleMouseClick(float mx, float my) override;
    void handleMouseDrag(float dx, float dy) override;
    void handleMouseWheel(float dy) override;
    void handleKeyDown(int key, int scancode, int mods) override;

    VkImage getOutputImage() const override { return _storageImage->getImage(); }
    VkExtent2D getOutputExtent() const override {
        return { _swapChain->getSwapChainExtent().width  * _supersampleScale,
                 _swapChain->getSwapChainExtent().height * _supersampleScale };
    }

    void buildUI() override;

private:

    // void createBottomLevelAccelerationStructures();
    // void createTopLevelAccelerationStructure();

    // Physical Device Properties / Features
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR _rayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR _accelerationStructureFeatures{};

    // Storage Image
    std::unique_ptr<StorageImage> _storageImage;

    // Uniform Buffer
    struct UniformData {
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
        glm::vec3 camPosition; float pad0;
        glm::vec3 lightPosition; float pad1;
        glm::vec3 lightU; float pad2;
        glm::vec3 lightV; float pad3;
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

    // Super Sampling Anti-Aliasing (SSAA)
    const uint32_t _supersampleScale = 2;

    // Camera
    std::unique_ptr<TurnTableCamera> _camera;
    bool _cameraOrbiting = false;

    // Time
    float _time = 0.0f;
    bool _isPaused = false;
    TimePoint _lastFrameTime = std::chrono::high_resolution_clock::now();

    // Scene graph (geometry templates + scene objects)
    std::unique_ptr<SceneGraph> _sceneGraph;

    // Top Level Acceleration Structure (TLAS)
    std::unique_ptr<TLAS> _tlas;
    void createTLAS();
    void updateTLAS();

    // Instance Data Buffer
    std::unique_ptr<Buffer> _instanceDataBuffer;
    void createInstanceDataBuffer();

    // Misc
    void saveSceneState(const std::string& filename);
    void loadSceneState(const std::string& filename);
};