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

/// @brief Enabled & required Vulkan instance extensions
static const std::vector<const char*> gInstanceExtensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
    HRI_VK_PLATFORM_SURFACE_EXTENSION_NAME,
};

/// @brief Enabled & required Vulkan device extensions
static const std::vector<const char*> gDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

RenderContext::RenderContext(RenderContextCreateInfo createInfo)
{
    assert(createInfo.surfaceCreateFunc != nullptr);
    // TODO: error reporting for entire function

    vkb::InstanceBuilder instanceBuilder = vkb::InstanceBuilder();
    instance = instanceBuilder
        .require_api_version(HRI_VK_API_VERSION)
        .set_engine_name(HRI_ENGINE_NAME)
        .set_engine_version(HRI_ENGINE_VERSION)
        .enable_extensions(gInstanceExtensions)
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

    // TODO: select gpu based on system capabilities & requirements (mainly check for hardware raytracing support)
    vkb::PhysicalDeviceSelector gpuSelector = vkb::PhysicalDeviceSelector(instance, surface);
    gpu = gpuSelector
        .require_present(true)
        .add_required_extensions(gDeviceExtensions)
        .select().value();

    // TODO: set up device queues properly
    vkb::DeviceBuilder deviceBuilder = vkb::DeviceBuilder(gpu);
    device = deviceBuilder
        .build().value();

    SwapchainPresentSetup presentSetup = RenderContext::getSwapPresentSetup(createInfo.vsyncMode);
    vkb::SwapchainBuilder swapchainBuilder = vkb::SwapchainBuilder(device);
    swapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .add_fallback_format(VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(presentSetup.presentMode)
        .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_required_min_image_count(presentSetup.imageCount)
        .build().value();

    // Set up context config based on passed params
    m_config.vsyncMode = createInfo.vsyncMode;
}

RenderContext::~RenderContext()
{
    vkb::destroy_swapchain(swapchain);
    vkb::destroy_device(device);
    vkb::destroy_surface(instance, surface);
    vkb::destroy_instance(instance);
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
        .set_old_swapchain(oldSwapchain)
        .build().value();

    vkb::destroy_swapchain(oldSwapchain);
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
