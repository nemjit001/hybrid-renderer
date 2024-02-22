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
		VkRenderPass renderPass,
		hri::DescriptorSetLayout& sceneDataSetLayout,
		hri::BatchedSceneData& batchedScene
	);

	virtual ~GBufferLayoutSubsystem() = default;

	virtual void record(hri::ActiveFrame& frame) const override;

	virtual void updatedBatchedScene(hri::BatchedSceneData& batchedScene);

protected:
	hri::BatchedSceneData& m_batchedScene;
};

class UISubsystem
	:
	public hri::IRenderSubsystem
{
public:
	UISubsystem(
		hri::RenderContext* ctx,
		hri::DescriptorSetAllocator* descriptorSetAllocator,
		hri::ShaderDatabase* shaderDB,
		VkRenderPass renderPass
	);

	virtual ~UISubsystem();

	virtual void record(hri::ActiveFrame& frame) const override;
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
		hri::DescriptorSetLayout& presentInputSetLayout
	);

	virtual ~PresentationSubsystem() = default;

	virtual void record(hri::ActiveFrame& frame) const override;
};
