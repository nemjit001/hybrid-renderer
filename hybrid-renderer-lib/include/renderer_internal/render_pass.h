#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "renderer_internal/shader_database.h"

namespace hri
{
	class BuiltinRenderPass
	{
	public:
		BuiltinRenderPass() = default;

		BuiltinRenderPass(RenderContext* ctx, ShaderDatabase* shaderDB);

		virtual ~BuiltinRenderPass();

		void createResources();

		void destroyResources();

		void execute(VkCommandBuffer commandBuffer, uint32_t activeSwapImage) const;

	private:
		GraphicsPipelineBuilder builtinPipelineBuilder() const;

	private:
		RenderContext* m_pCtx						= nullptr;
		const PipelineStateObject* m_pBuiltinPSO	= nullptr;
		std::vector<VkImageView> m_swapViews		= {};
		std::vector<VkFramebuffer> m_framebuffers	= {};
		VkRenderPass m_renderPass					= VK_NULL_HANDLE;
	};
}
