#pragma once

#include <string>
#include <map>
#include <vulkan/vulkan.h>

#include "renderer_internal/descriptor_management.h"
#include "renderer_internal/debug.h"
#include "renderer_internal/render_context.h"
#include "renderer_internal/render_core.h"
#include "renderer_internal/shader_database.h"

namespace hri
{
    /// @brief A Render Subsystem manages a render pipeline, layout, and subsystem descriptors.
    ///     A Render Subsystem bases its pipeline config on a render pass.
    /// @tparam _FrameInfo per frame info for subsystem.
    template<typename _FrameInfo>
    class IRenderSubsystem
    {
    public:
        /// @brief Create a new render subsystem.
        /// @param ctx Render Context to use.
        IRenderSubsystem(RenderContext& ctx);

        /// @brief Destroy this subsystem.
        virtual ~IRenderSubsystem();

        // Disallow copy behaviour
        IRenderSubsystem(const IRenderSubsystem&) = delete;
        IRenderSubsystem& operator=(const IRenderSubsystem&) = delete;

        /// @brief Record this subsystem's render commands.
        /// @param frame 
        /// @param frameInfo
        virtual void record(ActiveFrame& frame, _FrameInfo& frameInfo) const = 0;

        inline float timeDelta() const { return m_debug.timeDelta(); }

    protected:
        RenderContext& m_ctx;
        hri_debug::DebugLabelHandler m_debug;
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
        PipelineStateObject* m_pPSO = nullptr;
    };

    template<typename _FrameInfo>
    IRenderSubsystem<_FrameInfo>::IRenderSubsystem(RenderContext& ctx)
        :
        m_ctx(ctx),
        m_debug(m_ctx)
    {
        //
    }

    template<typename _FrameInfo>
    IRenderSubsystem<_FrameInfo>::~IRenderSubsystem()
    {
        vkDestroyPipelineLayout(m_ctx.device, m_layout, nullptr);
    }
}
