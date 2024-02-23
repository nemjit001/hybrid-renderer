#pragma once

#include "config.h"
#include "platform.h"

#if     HRI_PLATFORM_WINDOWS == 1
#define VK_USE_PLATFORM_WIN32_KHR
#elif   HRI_PLATFORM_UNIX == 1
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <functional>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#define HRI_VK_API_VERSION  VK_API_VERSION_1_3
#define HRI_ENGINE_VERSION  VK_MAKE_API_VERSION(0, HRI_ENGINE_VERSION_MAJOR, HRI_ENGINE_VERSION_MINOR, HRI_ENGINE_VERSION_PATCH)

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

    /// @brief The render context create info allows for specifying context settings & registering extensions for the created context.
    struct RenderContextCreateInfo
    {
        const char* appName                         = "NONAME";
        uint32_t appVersion                         = 0;
        HRISurfaceCreateFunc surfaceCreateFunc      = nullptr; 
        VSyncMode vsyncMode                         = VSyncMode::Disabled;
        std::vector<const char*> instanceExtensions = {};
        std::vector<const char*> deviceExtensions   = {};
    };

    /// @brief The swapchain present setup dictates available swap images for rendering, as well as the present mode.
    struct SwapchainPresentSetup
    {
        uint32_t imageCount;
        VkPresentModeKHR presentMode;
    };

    /// @brief A Device Queue is a queue on the logical device to which operations can be submitted.
    struct DeviceQueue
    {
        uint32_t family = VK_QUEUE_FAMILY_IGNORED;
        VkQueue handle  = VK_NULL_HANDLE;
    };

    /// @brief The context queue state creates and maintains the device queue setup.
    class RenderContextQueueState
    {
    public:
        /// @brief Create an empty queue state, has invalid queues stored.
        RenderContextQueueState() = default;

        /// @brief Create a new queue state.
        /// @param device The device for which to retrieve the queues.
        RenderContextQueueState(vkb::Device device);

    public:
        DeviceQueue graphicsQueue   = DeviceQueue{};
        DeviceQueue presentQueue    = DeviceQueue{};
    };

    /// @brief THe RenderContext manages the Vulkan instance, device, and swapchain state.
    class RenderContext
    {
    public:
        /// @brief Instantiate a new Render Context.
        /// @param createInfo Create info for this context.
        RenderContext(RenderContextCreateInfo& createInfo);

        /// @brief Destroy this Render Context instance.
        virtual ~RenderContext();

        // Disable copy behaviour due to vulkan object ownership management
        RenderContext(const RenderContext&) = delete;
        RenderContext& operator=(const RenderContext&) = delete;

        // Allow move semantics
        RenderContext(RenderContext&& other) noexcept;
        RenderContext& operator=(RenderContext&& other) noexcept;

        /// @brief Set the VSync mode for the swap chain. Recreates the swap chain.
        ///     NOTE: does not free any allocated swap images or views!
        /// @param vsyncMode The new VSync mode to use.
        void setVSyncMode(VSyncMode vsyncMode);

        /// @brief Recreate the swap chain, freeing old resources.
        ///     NOTE: does not free any allocated swap images or views!
        void recreateSwapchain();

        template<typename _PFn>
        _PFn getInstanceFunction(const char* name) const;

        template<typename _PFn>
        _PFn getDeviceFunction(const char* name) const;

        /// @brief Retrieve the swapchain format.
        /// @return The active swapchain format.
        inline VkFormat swapFormat() const { return swapchain.image_format; }

    private:
        /// @brief Release resources held by this context.
        void release();

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
        vkb::Instance instance          = vkb::Instance();
        VkSurfaceKHR surface            = VK_NULL_HANDLE;
        vkb::PhysicalDevice gpu         = vkb::PhysicalDevice();
        vkb::Device device              = vkb::Device();
        VmaAllocator allocator          = VK_NULL_HANDLE;
        vkb::Swapchain swapchain        = vkb::Swapchain();
        RenderContextQueueState queues  = RenderContextQueueState();

    private:
        /// @brief The Context Config maintains state needed for recreation of devices, swapchains, etc.
        struct ContextConfig
        {
            VSyncMode vsyncMode;
        };

        ContextConfig m_config = ContextConfig{};
    };

    template<typename _PFn>
    _PFn RenderContext::getInstanceFunction(const char* name) const
    {
        void* pFunction = vkGetInstanceProcAddr(instance, name);
        assert(pFunction != nullptr);
        
        return reinterpret_cast<_PFn>(pFunction);
    }

    template<typename _PFn>
    _PFn RenderContext::getDeviceFunction(const char* name) const
    {
        void* pFunction = vkGetDeviceProcAddr(device, name);
        assert(pFunction != nullptr);
        
        return reinterpret_cast<_PFn>(pFunction);
    }
}

