#include "GUI.h"
#include "VulkanHelper.h"


GUI::GUI(std::shared_ptr<VulkanContext> ctx, SDL_Window* window, VkFormat swapChainImageFormat)
    : _ctx(std::move(ctx)), _window(window)
{
    createDescriptorPool();
    createRenderPass(swapChainImageFormat);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // SDL3 backend
    ImGui_ImplSDL3_InitForVulkan(_window);

    // Vulkan backend
    QueueFamilyIndices queueFamilyIndices = VulkanHelper::findQueueFamilies(_ctx->physicalDevice, _ctx->surface);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion                         = VK_API_VERSION_1_3;
    initInfo.Instance                           = _ctx->instance;
    initInfo.PhysicalDevice                     = _ctx->physicalDevice;
    initInfo.Device                             = _ctx->device;
    initInfo.QueueFamily                        = queueFamilyIndices.graphicsFamily.value();
    initInfo.Queue                              = _ctx->graphicsQueue;
    initInfo.DescriptorPool                     = _imguiDescriptorPool;
    initInfo.MinImageCount                      = 2;
    initInfo.ImageCount                         = 2;
    initInfo.PipelineInfoMain.RenderPass        = _renderPass;
    initInfo.PipelineInfoMain.MSAASamples       = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    spdlog::info("ImGui initialized.");
}

GUI::~GUI()
{
    vkDeviceWaitIdle(_ctx->device);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    vkDestroyRenderPass(_ctx->device, _renderPass, nullptr);
    vkDestroyDescriptorPool(_ctx->device, _imguiDescriptorPool, nullptr);
}

void GUI::createDescriptorPool()
{
    // ImGui needs a pool with plenty of combined image samplers for font atlas
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 16;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = 16;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;

    if (vkCreateDescriptorPool(_ctx->device, &poolInfo, nullptr, &_imguiDescriptorPool) != VK_SUCCESS) {
        spdlog::error("GUI: Failed to create ImGui descriptor pool!");
    }
}

void GUI::createRenderPass(VkFormat swapChainImageFormat)
{
    // Load the existing blit result, draw ImGui on top, then transition to PRESENT
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapChainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;          // preserve blit result
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(_ctx->device, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
        spdlog::error("GUI: Failed to create ImGui render pass!");
    }
}


bool GUI::handleEvent(SDL_Event* event)
{
    return ImGui_ImplSDL3_ProcessEvent(event);
}

bool GUI::isCapturingMouse() const
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool GUI::isCapturingKeyboard() const
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

void GUI::beginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void GUI::buildUI()
{
    // Renderer-level controls (scene-independent)
    // e.g. scene selector, global settings
}

void GUI::recordToCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkExtent2D extent)
{
    ImGui::Render();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = _renderPass;
    renderPassInfo.framebuffer       = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = extent;
    renderPassInfo.clearValueCount   = 0; // loadOp = LOAD, no clear needed

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    vkCmdEndRenderPass(commandBuffer);
}
