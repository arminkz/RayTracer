#pragma once

#include "stdafx.h"
#include "VulkanContext.h"
#include "VulkanHelper.h"


struct RayTracingPipelineParams {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    std::string name;
};


class RayTracingPipeline {
public:
    RayTracingPipeline(std::shared_ptr<VulkanContext> ctx,
        const std::string& raygenShaderPath, 
        const std::string& missShaderPath, 
        const std::string& closestHitShaderPath,
        const RayTracingPipelineParams& params
    );
    ~RayTracingPipeline();

    VkPipeline getPipeline() const { return _pipeline; }
    VkPipelineLayout getPipelineLayout() const { return _pipelineLayout; }

    const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& getShaderGroups() { return _shaderGroups; }

private:
    std::shared_ptr<VulkanContext> _ctx;

    VkPipeline _pipeline = VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> _shaderGroups{};

    void createPipelineLayout(const RayTracingPipelineParams& params);
    void createRayTracingPipeline(
        const std::string& raygenShaderPath, 
        const std::string& missShaderPath, 
        const std::string& closestHitShaderPath,
        const RayTracingPipelineParams& params
    );
    
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readBinaryFile(const std::string& filename);

    std::string _name;
};