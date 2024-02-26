#include "renderer_internal/render_context.h"

#include "config.h"
#include "platform.h"

#include <cassert>
#include <cstdio>
#include <vector>
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#if     HRI_PLATFORM_WINDOWS == 1
#define HRI_VK_PLATFORM_SURFACE_EXTENSION_NAME  VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif   HRI_PLATFORM_UNIX == 1
#define HRI_VK_PLATFORM_SURFACE_EXTENSION_NAME  VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif

using namespace hri;

/// @brief Enabled & required default Vulkan instance extensions
static const std::vector<const char*> gInstanceExtensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
    HRI_VK_PLATFORM_SURFACE_EXTENSION_NAME,
};

/// @brief Enabled & required default Vulkan device extensions
static const std::vector<const char*> gDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

RenderContextQueueState::RenderContextQueueState(vkb::Device device)
{
    vkb::Result<uint32_t> transferIndexResult = device.get_dedicated_queue_index(vkb::QueueType::transfer);
    vkb::Result<VkQueue> transferQueueResult = device.get_dedicated_queue(vkb::QueueType::transfer);
    if (!transferQueueResult.has_value() || !transferIndexResult.has_value())
    {
        transferIndexResult = device.get_queue_index(vkb::QueueType::transfer);
        transferQueueResult = device.get_queue(vkb::QueueType::transfer);
    }

    graphicsQueue = DeviceQueue{
        device.get_queue_index(vkb::QueueType::graphics).value(),
        device.get_queue(vkb::QueueType::graphics).value(),
    };

    transferQueue = DeviceQueue{
        transferIndexResult.value(),
        transferQueueResult.value(),
    };

    presentQueue = DeviceQueue{
        device.get_queue_index(vkb::QueueType::present).value(),
        device.get_queue(vkb::QueueType::present).value(),
    };
}

RenderContext::RenderContext(RenderContextCreateInfo& createInfo)
{
    assert(createInfo.surfaceCreateFunc != nullptr);
    // TODO: error reporting for entire function

    createInfo.instanceExtensions.insert(createInfo.instanceExtensions.end(), gInstanceExtensions.begin(), gInstanceExtensions.end());
    createInfo.deviceExtensions.insert(createInfo.deviceExtensions.end(), gDeviceExtensions.begin(), gDeviceExtensions.end());

    vkb::InstanceBuilder instanceBuilder = vkb::InstanceBuilder();
    instance = instanceBuilder
        .require_api_version(HRI_VK_API_VERSION)
        .set_app_name(createInfo.appName)
        .set_app_version(createInfo.appVersion)
        .set_engine_name(HRI_ENGINE_NAME)
        .set_engine_version(HRI_ENGINE_VERSION)
        .enable_extensions(createInfo.instanceExtensions)
#if HRI_DEBUG == 1
        .add_debug_messenger_severity(
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        )
        .add_debug_messenger_type(
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        )
        .enable_validation_layers(true)
        .set_debug_callback(RenderContext::debugCallback)
#endif
        .build().value();

    HRI_VK_CHECK(createInfo.surfaceCreateFunc(instance, &surface));

    vkb::PhysicalDeviceSelector gpuSelector = vkb::PhysicalDeviceSelector(instance, surface)
        .require_present(true)
        .add_required_extensions(createInfo.deviceExtensions)
        .set_required_features(createInfo.deviceFeatures)
        .set_required_features_11(createInfo.deviceFeatures11)
        .set_required_features_12(createInfo.deviceFeatures12)
        .set_required_features_13(createInfo.deviceFeatures13);

    for (auto const& feature : createInfo.extensionFeatures)
        gpuSelector.add_required_extension_features(feature);

    gpu = gpuSelector.select().value();

    vkb::DeviceBuilder deviceBuilder = vkb::DeviceBuilder(gpu);
    device = deviceBuilder
        .build().value();

    VmaAllocatorCreateInfo allocatorCreateInfo = VmaAllocatorCreateInfo{};
    allocatorCreateInfo.flags = 0;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.physicalDevice = gpu;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.vulkanApiVersion = HRI_VK_API_VERSION;
    HRI_VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &allocator));

    SwapchainPresentSetup presentSetup = RenderContext::getSwapPresentSetup(createInfo.vsyncMode);
    vkb::SwapchainBuilder swapchainBuilder = vkb::SwapchainBuilder(device);
    swapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .add_fallback_format(VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(presentSetup.presentMode)
        .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_required_min_image_count(presentSetup.imageCount)
        .set_image_usage_flags( // Some sensible default flags for swap image usage
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT
        )
        .build().value();

    // Initialize device queues
    queues = RenderContextQueueState(device);

    // Set up context config based on passed params
    m_config.vsyncMode = createInfo.vsyncMode;
}

RenderContext::~RenderContext()
{
    release();
}

RenderContext::RenderContext(RenderContext&& other) noexcept
    :
    instance(other.instance),
    surface(other.surface),
    gpu(other.gpu),
    device(other.device),
    allocator(other.allocator),
    swapchain(other.swapchain),
    queues(other.queues)
{
    other.instance = vkb::Instance();
    other.surface = VK_NULL_HANDLE;
    other.gpu = vkb::PhysicalDevice();
    other.device = vkb::Device();
    other.allocator = VK_NULL_HANDLE;
    other.swapchain = vkb::Swapchain();
}

RenderContext& RenderContext::operator=(RenderContext&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    release();
    instance = other.instance;
    surface = other.surface;
    gpu = other.gpu;
    device = other.device;
    allocator = other.allocator;
    swapchain = other.swapchain;
    queues = other.queues;

    other.instance = vkb::Instance();
    other.surface = VK_NULL_HANDLE;
    other.gpu = vkb::PhysicalDevice();
    other.device = vkb::Device();
    other.allocator = VK_NULL_HANDLE;
    other.swapchain = vkb::Swapchain();

    return *this;
}

void RenderContext::setVSyncMode(VSyncMode vsyncMode)
{
    // Nothing to do here
    if (m_config.vsyncMode == vsyncMode)
        return;

    m_config.vsyncMode = vsyncMode;
    recreateSwapchain();
}

void RenderContext::recreateSwapchain()
{
    HRI_VK_CHECK(vkDeviceWaitIdle(device));

    SwapchainPresentSetup presentSetup = RenderContext::getSwapPresentSetup(m_config.vsyncMode);
    vkb::Swapchain oldSwapchain = swapchain;

    vkb::SwapchainBuilder swapchainBuilder = vkb::SwapchainBuilder(device);
    swapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ oldSwapchain.image_format, oldSwapchain.color_space })
        .set_desired_present_mode(presentSetup.presentMode)
        .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_required_min_image_count(presentSetup.imageCount)
        .set_image_usage_flags(oldSwapchain.image_usage_flags)
        .set_old_swapchain(oldSwapchain)
        .build().value();

    vkb::destroy_swapchain(oldSwapchain);
}

void RenderContext::release()
{
    vkb::destroy_swapchain(swapchain);
    vmaDestroyAllocator(allocator);
    vkb::destroy_device(device);
    vkb::destroy_surface(instance, surface);
    vkb::destroy_instance(instance);
}

SwapchainPresentSetup RenderContext::getSwapPresentSetup(VSyncMode vsyncMode)
{
    // Select image count & present mode based on VSync mode
    uint32_t swapImageCount = 0;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    switch (vsyncMode)
    {
    case VSyncMode::Disabled:
        swapImageCount = HRI_DEFAULT_SWAP_IMAGE_COUNT;
        presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        break;
    case VSyncMode::DoubleBuffering:
        swapImageCount = 2;
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
        break;
    case VSyncMode::TripleBuffering:
        swapImageCount = 3;
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
        break;
    }

    return SwapchainPresentSetup{
        swapImageCount,
        presentMode,
    };
}

VKAPI_ATTR VkBool32 VKAPI_CALL RenderContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    fprintf(stderr, "[Vulkan Debug] %s\n", pCallbackData->pMessage);    
    return VK_FALSE;
}
