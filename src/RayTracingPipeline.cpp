#include "RayTracingPipeline.h"
#include "VulkanRT.h"

RayTracingPipeline::RayTracingPipeline(std::shared_ptr<VulkanContext> ctx,
    const std::string& raygenShaderPath, 
    const std::string& missShaderPath, 
    const std::string& closestHitShaderPath,
    const RayTracingPipelineParams& params)
    : _ctx(std::move(ctx)), _name(params.name)
{
    createPipelineLayout(params);
    createRayTracingPipeline(raygenShaderPath, missShaderPath, closestHitShaderPath, params);
}


RayTracingPipeline::~RayTracingPipeline()
{
    if (_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(_ctx->device, _pipeline, nullptr);
        _pipeline = VK_NULL_HANDLE;
    }
    if (_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(_ctx->device, _pipelineLayout, nullptr);
        _pipelineLayout = VK_NULL_HANDLE;
    }
}


void RayTracingPipeline::createPipelineLayout(const RayTracingPipelineParams& params)
{
    // Create the pipeline layout with the descriptor set layout
    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.pNext = nullptr;
    pipelineLayoutCI.flags = 0;
    pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>(params.descriptorSetLayouts.size());
    pipelineLayoutCI.pSetLayouts = params.descriptorSetLayouts.empty() ? nullptr : params.descriptorSetLayouts.data();
    pipelineLayoutCI.pushConstantRangeCount = 0;
    pipelineLayoutCI.pPushConstantRanges = nullptr;
    if (vkCreatePipelineLayout(_ctx->device, &pipelineLayoutCI, nullptr, &_pipelineLayout) != VK_SUCCESS) {
        spdlog::error("Failed to create pipeline layout!");
        throw std::runtime_error("Failed to create pipeline layout!");
    }
}


void RayTracingPipeline::createRayTracingPipeline(
    const std::string& raygenShaderPath,
    const std::string& missShaderPath,
    const std::string& closestHitShaderPath,  
    const RayTracingPipelineParams& params)
{

    // Setup ray tracing shader groups
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    // Load shaders from files
    auto raygenShaderCode = readBinaryFile(raygenShaderPath);
    auto missShaderCode = readBinaryFile(missShaderPath);
    auto closestHitShaderCode = readBinaryFile(closestHitShaderPath);

    // Create shader modules
    VkShaderModule raygenShaderModule = createShaderModule(raygenShaderCode);
    VkShaderModule missShaderModule = createShaderModule(missShaderCode);
    VkShaderModule closestHitShaderModule = createShaderModule(closestHitShaderCode);

    // Raygen shader stage info
    VkPipelineShaderStageCreateInfo raygenShaderStageInfo{};
    raygenShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    raygenShaderStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    raygenShaderStageInfo.module = raygenShaderModule;
    raygenShaderStageInfo.pName = "main";

    // Miss shader stage info
    VkPipelineShaderStageCreateInfo missShaderStageInfo{};
    missShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    missShaderStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missShaderStageInfo.module = missShaderModule;
    missShaderStageInfo.pName = "main";

    // Closest hit shader stage info
    VkPipelineShaderStageCreateInfo closestHitShaderStageInfo{};
    closestHitShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    closestHitShaderStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    closestHitShaderStageInfo.module = closestHitShaderModule;
    closestHitShaderStageInfo.pName = "main";

    // Ray generation group
    {
        shaderStages.push_back(raygenShaderStageInfo);
        VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
        shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        _shaderGroups.push_back(shaderGroup);
    }

    // Miss group
    {
        shaderStages.push_back(missShaderStageInfo);
        VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
        shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        _shaderGroups.push_back(shaderGroup);
    }

    // Closest hit group
    {
        shaderStages.push_back(closestHitShaderStageInfo);
        VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        _shaderGroups.push_back(shaderGroup);
    }

    // Create the ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
    rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    rayTracingPipelineCI.pNext = nullptr;
    rayTracingPipelineCI.flags = 0;
    rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayTracingPipelineCI.pStages = shaderStages.data();
    rayTracingPipelineCI.groupCount = static_cast<uint32_t>(_shaderGroups.size());
    rayTracingPipelineCI.pGroups = _shaderGroups.data();
    rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
    rayTracingPipelineCI.layout = _pipelineLayout;
    rayTracingPipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    rayTracingPipelineCI.basePipelineIndex = -1;

    if (vkrt::vkCreateRayTracingPipelinesKHR(_ctx->device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline!");
    }else {
        spdlog::info("Ray tracing pipeline created successfully {}", _name != "" ? fmt::format("({})", _name) : "");
    }

    // Cleanup shader modules
    vkDestroyShaderModule(_ctx->device, raygenShaderModule, nullptr);
    vkDestroyShaderModule(_ctx->device, missShaderModule, nullptr);
    vkDestroyShaderModule(_ctx->device, closestHitShaderModule, nullptr);
}

VkShaderModule RayTracingPipeline::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(_ctx->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

std::vector<char> RayTracingPipeline::readBinaryFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", filename);
        return {};
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}