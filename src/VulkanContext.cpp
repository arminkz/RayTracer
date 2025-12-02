#include "VulkanContext.h"
#include "VulkanRT.h"


VulkanContext::VulkanContext(SDL_Window* window)
    : window(window)
{
    createVulkanInstance();
    setupDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    loadVulkanRTFunctions();
    createDescriptorPool();
    createCommandPool();
}

VulkanContext::~VulkanContext() {
    spdlog::info("Destroying Vulkan context...");
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);

    if (debugMessenger) {
        auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyDebugUtilsMessengerEXT != nullptr) {
            destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
    }

    vkDestroyInstance(instance, nullptr);
}

void VulkanContext::createVulkanInstance() {
    // Initialize Vulkan instance
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    bool enableVL = true;

#if __APPLE__
    enableVL = false; // Disable validation layers on macOS for now (MoltenVK issues)
#endif

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    // Check for validation layer support
    _validationLayersAvailable = isInstanceLayerAvailable("VK_LAYER_KHRONOS_validation");
    if (!_validationLayersAvailable || !enableVL) {
        spdlog::warn("Validation layers not available, disabling validation layers.");
        instanceCreateInfo.enabledLayerCount = 0;
        instanceCreateInfo.ppEnabledLayerNames = nullptr;
    }else {
        spdlog::info("Validation layers are available!");
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    }

    // Required extensions 
    // SDL extensions
    uint32_t sdlExtensionCount = 0;
    const char * const *sdlExtensionsRaw = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    std::vector<const char*> requiredExtensions(sdlExtensionsRaw, sdlExtensionsRaw + sdlExtensionCount);

    // Debug Utils extension
    if(!isInstanceExtensionAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
        spdlog::warn("Debug Utils extension not available!");
    }else {
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Other extensions can be added here as needed
    if(!isInstanceExtensionAvailable(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        spdlog::warn("VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME not available!");
    }else {
        requiredExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();

    // In macOS, we need to enable the portability enumeration flag
    // Since we are using SDL extensions, the extension should already be included. we just need to set the flag.
#ifdef __APPLE__
    instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    // Print Enabled Extensions
    spdlog::debug("Enabled Vulkan Instance Extensions:");
    for (const char* extension : requiredExtensions) {
        spdlog::debug("  {}", extension);
    }

    // Create Vulkan instance
    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        spdlog::error("Failed to create Vulkan instance: {}", static_cast<int>(result));
        throw std::runtime_error("Failed to create Vulkan instance!");
    }

    spdlog::info("Vulkan instance created successfully");
}

void VulkanContext::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr; // Optional

    auto createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (createDebugUtilsMessengerEXT != nullptr) {
        if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            spdlog::error("Failed to set up debug messenger!");
        } else {
            spdlog::info("Debug messenger set up successfully");
        }
    } else {
        spdlog::error("vkGetInstanceProcAddr failed to find vkCreateDebugUtilsMessengerEXT function!");
    }
}

void VulkanContext::createSurface(SDL_Window* window) {
    // A surface is a platform-specific representation of the window where Vulkan will render its output.
    // its a window tied to a swapchain.
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        throw std::runtime_error("Failed to create Vulkan surface!");
    }
    spdlog::info("Vulkan surface created successfully");
}

void VulkanContext::pickPhysicalDevice() {
    // Pick a suitable physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            spdlog::info("Found Suitable Discrete GPU: {}", deviceProperties.deviceName);
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        spdlog::warn("No suitable discrete GPU found, trying fallback to any integrated GPU!");
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            if (isDeviceSuitable(device, true)) {
                physicalDevice = device;
                spdlog::info("Found Suitable iGPU: {}", deviceProperties.deviceName);
                break;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }
}

void VulkanContext::createLogicalDevice() {
    // Queue family indices
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    // Store the queue family with graphics support
    std::optional<uint32_t> graphicsFamily;
    // Store the queue family with present support
    std::optional<uint32_t> presentFamily;

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            presentFamily = i;
        }
    }

    // Create logical device
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    // Specify the queue create info for graphics and present queues
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily.value(), presentFamily.value()};
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

    // Specify the device extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
    };
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Specify the device features
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE; // Enable anisotropic filtering
    deviceFeatures.sampleRateShading = VK_TRUE; // Enable sample rate shading
    deviceFeatures.shaderInt64 = VK_TRUE;       // Enable 64-bit integer support in shaders

    // Enable buffer device address feature (required for ray tracing)
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

    // Enable ray tracing pipeline features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingPipelineFeatures.pNext = &bufferDeviceAddressFeatures;

    // Enable acceleration structure features
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructureFeatures.accelerationStructure = VK_TRUE;
    accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

    // Chain the features
    deviceCreateInfo.pNext = &accelerationStructureFeatures;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    spdlog::info("Logical device created successfully");

    spdlog::info("Ray tracing function pointers loaded successfully");

    // Get the graphics queue handle
    vkGetDeviceQueue(device, graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily.value(), 0, &presentQueue);
}

void VulkanContext::createDescriptorPool() {

    // Descriptor usage counts per type
    uint32_t totalUBOs = MAX_FRAMES_IN_FLIGHT;
    uint32_t totalSSBOs = 10;
    //uint32_t totalSamplers = 70;
    uint32_t totalAccelerationStructures = MAX_FRAMES_IN_FLIGHT; // One per frame
    uint32_t totalStorageImages = MAX_FRAMES_IN_FLIGHT;          // One per frame
    uint32_t maxSets = MAX_FRAMES_IN_FLIGHT;                     // One per frame

    std::vector<VkDescriptorPoolSize> poolSizes = {
        //{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, totalSamplers }
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, totalAccelerationStructures },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, totalStorageImages },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, totalUBOs },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, totalSSBOs }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    spdlog::info("Descriptor pool created successfully");
    }

void VulkanContext::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = VulkanHelper::findQueueFamilies(physicalDevice, surface);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    spdlog::info("Command pool created successfully");
}

void VulkanContext::loadVulkanRTFunctions() {
    // Load ray tracing function pointers
    vkrt::vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    vkrt::vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    vkrt::vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    vkrt::vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkrt::vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkrt::vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR"));
    vkrt::vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    vkrt::vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
    vkrt::vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkrt::vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device, bool fallback) {
    // Check if the device is suitable for our needs (graphics, compute, etc.)
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // Check if the device is discrete GPU
    bool isDiscreteGPU = (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

    // Check if the device supports certain extensions (including ray tracing extensions)
    std::set<std::string> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
    };
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    bool hasRequiredExtensions = requiredExtensions.empty();

    // Check if the device swapchain is adequate
    SwapChainSupportDetails swapChainSupport = VulkanHelper::querySwapChainSupport(device, surface);
    bool swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

    // Log device name + suitability
    spdlog::debug("Evaluating GPU: {}", deviceProperties.deviceName);
    spdlog::debug(" - Discrete GPU: {}", isDiscreteGPU ? "Yes" : "No");
    spdlog::debug(" - Required Extensions: {}", hasRequiredExtensions ? "Yes" : "No");
    spdlog::debug(" - Swapchain Adequate: {}", swapChainAdequate ? "Yes" : "No");
    
    // AND all the checks together
    // if fallback is true, we allow non-discrete GPUs
    if (!fallback) {
        return isDiscreteGPU && hasRequiredExtensions && swapChainAdequate;
    }
    return hasRequiredExtensions && swapChainAdequate;
}

bool VulkanContext::isInstanceLayerAvailable(const char* layerName) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto& layer : availableLayers) {
        if (strcmp(layerName, layer.layerName) == 0) {
            return true;
        }
    }
    return false;
}

bool VulkanContext::isInstanceExtensionAvailable(const char* extensionName) {
    uint32_t extensionCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    for (const auto& extension : availableExtensions) {
        if (strcmp(extensionName, extension.extensionName) == 0) {
            return true;
        }
    }
    return false;
}


VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    spdlog::error("{}", pCallbackData->pMessage);

    return VK_FALSE;
}

void VulkanContext::printVulkanInfo() {
    spdlog::info("--------------------------------");
    spdlog::info("Vulkan API version: {}.{}",  VK_API_VERSION_MAJOR(VK_API_VERSION_1_3), VK_API_VERSION_MINOR(VK_API_VERSION_1_3));
    spdlog::info("--------------------------------");

    // Print available layers
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    spdlog::info("Available layers:");
    for (const auto& layer : availableLayers) {
        spdlog::info("  {}", layer.layerName);
    }

    spdlog::info("------------------------------------");
    
    // Print available extensions
    uint32_t extensionCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
    spdlog::info("Available extensions:");
    for (const auto& extension : availableExtensions) {
        spdlog::info("  {}", extension.extensionName);
    }

    spdlog::info("------------------------------------");
    spdlog::info("Validation layers available: {}", _validationLayersAvailable ? "true" : "false");
}