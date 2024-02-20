#pragma once

#include <hybrid_renderer.h>

class GBufferLayoutSubsystem
	:
	public hri::IRenderSubsystem
{
public:
	GBufferLayoutSubsystem(hri::RenderContext* ctx, hri::ShaderDatabase* shaderDB, VkRenderPass renderPass);

	virtual ~GBufferLayoutSubsystem();

	virtual void record(hri::ActiveFrame& frame) const override;

protected:
	hri::DescriptorSetLayout m_sceneDescriptorSetLayout;
	hri::DescriptorSetLayout m_objectDescriptorSetLayout;
};

class PresentationSubsystem
	:
	public hri::IRenderSubsystem
{
public:
	PresentationSubsystem(hri::RenderContext* ctx, hri::ShaderDatabase* shaderDB, VkRenderPass renderPass);

	virtual ~PresentationSubsystem();

	virtual void record(hri::ActiveFrame& frame) const override;

protected:
	hri::DescriptorSetLayout m_renderResultDescriptorSetLayout;
};
