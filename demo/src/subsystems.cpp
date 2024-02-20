#include "subsystems.h"

#include <hybrid_renderer.h>

GBufferLayoutSubsystem::GBufferLayoutSubsystem(
	hri::RenderContext* ctx,
	hri::ShaderDatabase* shaderDB,
	VkRenderPass renderPass
)
	:
	hri::IRenderSubsystem(ctx, shaderDB, renderPass)
{
	//
}

GBufferLayoutSubsystem::~GBufferLayoutSubsystem()
{
	//
}

void GBufferLayoutSubsystem::record(hri::ActiveFrame& frame) const
{
	//
}

PresentationSubsystem::PresentationSubsystem(
	hri::RenderContext* ctx,
	hri::ShaderDatabase* shaderDB,
	VkRenderPass renderPass
)
	:
	hri::IRenderSubsystem(ctx, shaderDB, renderPass)
{
	//
}

PresentationSubsystem::~PresentationSubsystem()
{
	//
}

void PresentationSubsystem::record(hri::ActiveFrame& frame) const
{
	//
}
