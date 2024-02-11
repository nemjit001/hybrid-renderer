#pragma once

#include "platform.h"

#if     HRI_PLATFORM_WINDOWS == 1
#define VK_USE_PLATFORM_WIN32_KHR
#elif   HRI_PLATFORM_UNIX == 1
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <functional>
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#define HRI_VK_API_VERSION  VK_API_VERSION_1_3
#define HRI_ENGINE_NAME     "HybridRenderEngine"
#define HRI_ENGINE_VERSION  VK_MAKE_API_VERSION(0, 1, 0, 0)

#define HRI_DEFAULT_SWAP_IMAGE_COUNT 3

#define HRI_VK_CHECK(result)    (void)((result == VK_SUCCESS) || (abort(), 0))
#define HRI_SIZEOF_ARRAY(a)     (sizeof(a) / sizeof(a[0]))

namespace hri
{
    /// @brief The HRI Surface Create function allows for api independent vulkan surface creation.
    typedef std::function<VkResult(vkb::Instance, VkSurfaceKHR*)> HRISurfaceCreateFunc;

    /// @brief VSync mode to use for rendering operations.
    enum class VSyncMode
    {
        Disabled,
        DoubleBuffering,
        TripleBuffering,
    };

    /// @brief The render context create info allows for specifying context settings.
    struct RenderContextCreateInfo
    {
        HRISurfaceCreateFunc surfaceCreateFunc  = nullptr; 
        VSyncMode vsyncMode                     = VSyncMode::Disabled;
    };

    /// @brief The swapchain present setup dictates available swap images for rendering, as well as the present mode.
    struct SwapchainPresentSetup
    {
        uint32_t imageCount;
        VkPresentModeKHR presentMode;
    };

    /// @brief THe RenderContext manages the Vulkan instance, device, and swapchain state.
    class RenderContext
    {
    public:
        /// @brief Instantiate a new Render Context.
        /// @param createInfo Create info for this context.
        RenderContext(RenderContextCreateInfo createInfo);

        /// @brief Destroy this Render Context instance.
        virtual ~RenderContext();

        // Disable copy behaviour due to vulkan object ownership management
        RenderContext(const RenderContext&) = delete;
        RenderContext& operator=(const RenderContext&) = delete;

        /// @brief Set the VSync mode for the swap chain. Recreates the swap chain.
        ///     NOTE: does not free any allocated swap images or views!
        /// @param vsyncMode The new VSync mode to use.
        void setVSyncMode(VSyncMode vsyncMode);

        /// @brief Recreate the swap chain, freeing old resources.
        ///     NOTE: does not free any allocated swap images or views!
        void recreateSwapchain();

    private:
        static SwapchainPresentSetup getSwapPresentSetup(VSyncMode vsyncMode);

        /// @brief Debug callback for the Vulkan API
        /// @param severity 
        /// @param messageTypes 
        /// @param pCallbackData 
        /// @param pUserData 
        /// @return 
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT messageTypes,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
        );

    public:
        vkb::Instance instance      = vkb::Instance();
        VkSurfaceKHR surface        = VK_NULL_HANDLE;
        vkb::PhysicalDevice gpu     = vkb::PhysicalDevice();
        vkb::Device device          = vkb::Device();
        vkb::Swapchain swapchain    = vkb::Swapchain();

    private:
        /// @brief The Context Config maintains state needed for recreation of devices, swapchains, etc.
        struct ContextConfig
        {
            VSyncMode vsyncMode;
        };

        ContextConfig m_config = ContextConfig{};
    };
}

