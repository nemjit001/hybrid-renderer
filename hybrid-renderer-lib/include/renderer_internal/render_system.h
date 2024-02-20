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
    class IRenderSubsystem;

    // TODO: create RenderPassIO manager that has a descriptor set & input views / output views & samplers for views
    // TODO: create RenderPassIOBuilder that allows for high level IO construction & specifying sampler modes per input.
    class RenderPassIO;
    class RenderPassIOBuilder;

    class RenderSubsystemManager
    {
    public:
        RenderSubsystemManager(RenderContext* ctx);

        virtual ~RenderSubsystemManager() = default;

        virtual void recordFrame(ActiveFrame& frame) const;

        void registerSubsystem(const std::string& name, IRenderSubsystem* subsystem, RenderPassIO* subsystemIO);

    private:
        RenderContext* m_pCtx = nullptr;
        std::unordered_map<std::string, RenderPassIO*> m_subsystemIO = {};
        std::unordered_map<std::string, IRenderSubsystem*> m_subsystems = {};
    };

	class IRenderSubsystem
	{
	public:
        IRenderSubsystem(RenderContext* ctx, ShaderDatabase* shaderDB, VkRenderPass renderPass);

        virtual ~IRenderSubsystem() = default;

        virtual void record(ActiveFrame& frame) const = 0;

	protected:
		RenderContext* m_pCtx = nullptr;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
        PipelineStateObject* m_pPSO = nullptr;
	};
}
