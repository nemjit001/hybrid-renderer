#pragma once

#include <string>
#include <map>
#include <vulkan/vulkan.h>

#include "renderer_internal/descriptor_management.h"
#include "renderer_internal/render_context.h"
#include "renderer_internal/render_core.h"
#include "renderer_internal/shader_database.h"

namespace hri
{
    /// @brief A Render Subsystem manages a render pipeline, layout, and subsystem descriptors.
    ///     A Render Subsystem bases its pipeline config on a render pass.
    class IRenderSubsystem
    {
    public:
        /// @brief Create a new render subsystem.
        /// @param ctx Render Context to use.
        IRenderSubsystem(RenderContext* ctx, DescriptorSetAllocator* descriptorSetAllocator);

        /// @brief Destroy this subsystem.
        virtual ~IRenderSubsystem();

        /// @brief Record this subsystem's render commands.
        /// @param frame 
        virtual void record(ActiveFrame& frame) const = 0;

    protected:
        RenderContext* m_pCtx = nullptr;
        DescriptorSetAllocator* m_pDescriptorSetAllocator = nullptr;
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
        PipelineStateObject* m_pPSO = nullptr;
    };

    /// @brief The Render Subsystem Manager manages subsystems, allowing registration by name.
    ///     Different subsystem commands can be recorded using the subsystem manager.
    class RenderSubsystemManager
    {
    public:
        /// @brief Create a new subsystem manager.
        /// @param ctx Render Context to use.
        RenderSubsystemManager(RenderContext* ctx);

        /// @brief Destroy this Subsystem Manager.
        virtual ~RenderSubsystemManager() = default;

        /// @brief Record a subsystem's render commands.
        /// @param name Name of the subsystem to record commands for.
        /// @param frame Active Frame given by Render Core.
        virtual void recordSubsystem(const std::string& name, ActiveFrame& frame) const;

        /// @brief Register a new subsystem.
        /// @param name Name of the subsystem, MUST be unique.
        /// @param subsystem Subsystem to register.
        void registerSubsystem(const std::string& name, IRenderSubsystem* subsystem);

    private:
        RenderContext* m_pCtx = nullptr;
        std::unordered_map<std::string, IRenderSubsystem*> m_subsystems = {};
    };
}
