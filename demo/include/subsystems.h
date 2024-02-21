#pragma once

#include <hybrid_renderer.h>

class GBufferLayoutSubsystem
	:
	public hri::IRenderSubsystem
{
public:
	GBufferLayoutSubsystem(
		hri::RenderContext* ctx,
		hri::DescriptorSetAllocator* descriptorSetAllocator,
		hri::ShaderDatabase* shaderDB,
		VkRenderPass renderPass
	);

	virtual ~GBufferLayoutSubsystem() = default;

	virtual void record(hri::ActiveFrame& frame) const override;

protected:
	//
};

class PresentationSubsystem
	:
	public hri::IRenderSubsystem
{
public:
	PresentationSubsystem(
		hri::RenderContext* ctx,
		hri::DescriptorSetAllocator* descriptorSetAllocator,
		hri::ShaderDatabase* shaderDB,
		VkRenderPass renderPass,
		hri::DescriptorSetLayout& globalSetLayout,
		hri::DescriptorSetManager& globalDescriptorSet
	);

	virtual ~PresentationSubsystem() = default;

	virtual void record(hri::ActiveFrame& frame) const override;

protected:
	hri::DescriptorSetManager& m_globalDescriptorSet;
};
