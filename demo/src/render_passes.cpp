#include "render_passes.h"

#include <hybrid_renderer.h>
#include <imgui_impl_vulkan.h>
#include <memory>

#include "detail/raytracing.h"
#include "scene.h"

// --- IRenderPass ---

IRenderPass::IRenderPass(hri::RenderContext& ctx)
	:
	context(ctx)
{
	//
}

void IRenderPass::prepareFrame(CommonResources& resources)
{
	// Don't do anything by default
}

// --- Path Tracing Pass ---

PathTracingPass::PathTracingPass(raytracing::RayTracingContext& ctx, hri::ShaderDatabase& shaderDB)
	:
	IRenderPass(ctx.renderContext),
	rtContext(ctx)
{
	recreateResources(context.swapchain.extent);

	hri::PipelineLayoutBuilder layoutBuilder(context);
	m_layout = layoutBuilder
		// TODO: set shader descriptor layouts here
		.build();

	hri::Shader* pRayGen = shaderDB.registerShader("PathTracingRayGen", hri::Shader::loadFile(context, "shaders/pt.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
	hri::Shader* pMiss = shaderDB.registerShader("PathTracingMiss", hri::Shader::loadFile(context, "shaders/pt.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
	hri::Shader* pCHit = shaderDB.registerShader("PathTracingCHit", hri::Shader::loadFile(context, "shaders/pt.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));

	raytracing::RayTracingPipelineBuilder pipelineBuilder(rtContext);
	pipelineBuilder
		.addShaderStage(pRayGen->stage, pRayGen->module)
		.addShaderStage(pMiss->stage, pMiss->module)
		.addShaderStage(pCHit->stage, pCHit->module)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1)
		.addRayTracingShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2)
		.setMaxRecursionDepth()
		.setLayout(m_layout);

	m_pPSO = shaderDB.registerPipeline("PathTracingPipeline", VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineBuilder.build());
	m_SBT = std::unique_ptr<raytracing::ShaderBindingTable>(new raytracing::ShaderBindingTable(rtContext, m_pPSO->pipeline, pipelineBuilder));
}

PathTracingPass::~PathTracingPass()
{
	vkDestroyPipelineLayout(context.device, m_layout, nullptr);
}

void PathTracingPass::drawFrame(hri::ActiveFrame& frame)
{
	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "Path Tracing Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	VkImageMemoryBarrier2 renderResultBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	renderResultBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR;
	renderResultBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	renderResultBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	renderResultBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	renderResultBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	renderResultBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	renderResultBarrier.image = renderResult->image;
	renderResultBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderResultBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderResultBarrier.subresourceRange = hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	frame.pipelineBarrier({ renderResultBarrier });

	VkStridedDeviceAddressRegionKHR raygen = m_SBT->getRegion(raytracing::ShaderBindingTable::SGRayGen);
	VkStridedDeviceAddressRegionKHR miss = m_SBT->getRegion(raytracing::ShaderBindingTable::SGMiss);
	VkStridedDeviceAddressRegionKHR hit = m_SBT->getRegion(raytracing::ShaderBindingTable::SGHit);
	VkStridedDeviceAddressRegionKHR call = m_SBT->getRegion(raytracing::ShaderBindingTable::SGCall);

	vkCmdBindPipeline(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_pPSO->pipeline
	);

	// TODO: once descriptor set updating & binding works, uncomment this
	//rtContext.rayTracingDispatch.vkCmdTraceRays(
	//	frame.commandBuffer,
	//	&raygen,
	//	&miss,
	//	&hit,
	//	&call,
	//	context.swapchain.extent.width,
	//	context.swapchain.extent.height,
	//	1
	//);

	renderResultBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	renderResultBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	renderResultBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	renderResultBarrier.dstAccessMask = 0;
	renderResultBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	renderResultBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	frame.pipelineBarrier({ renderResultBarrier });

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
	debug.cmdEndLabel(frame.commandBuffer);
}

void PathTracingPass::recreateResources(VkExtent2D resolution)
{
	renderResult = std::unique_ptr<hri::ImageResource>(new hri::ImageResource(
		context,
		VK_IMAGE_TYPE_2D,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_SAMPLE_COUNT_1_BIT,
		VkExtent3D{ resolution.width, resolution.height, 1 },
		1,
		1,
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT
	));

	renderResult->createView(VK_IMAGE_VIEW_TYPE_2D, hri::ImageResource::DefaultComponentMapping(), hri::ImageResource::SubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));
}

// --- PRESENT PASS ---

PresentPass::PresentPass(hri::RenderContext& ctx, hri::ShaderDatabase& shaderDB)
	:
	IRenderPass(ctx)
{
	hri::RenderPassBuilder passBuilder(ctx);
	passBuilder
		.addAttachment(
			ctx.swapFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED
		)
		.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	passResources = std::make_unique<hri::SwapchainPassResourceManager>(ctx, passBuilder.build());
	passResources->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });

	hri::PipelineLayoutBuilder layoutBuilder(context);
	m_layout = layoutBuilder
		// TODO: set input descriptor layouts
		.build();

	shaderDB.registerShader("FullscreenQuadVert", hri::Shader::loadFile(context, "shaders/fullscreen_quad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
	shaderDB.registerShader("PresentFrag", hri::Shader::loadFile(context, "shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

	VkExtent2D swapExtent = context.swapchain.extent;
	hri::GraphicsPipelineBuilder pipelineBuilder = hri::GraphicsPipelineBuilder{};
	pipelineBuilder.vertexInputBindings = {};
	pipelineBuilder.vertexInputAttributes = {};
	pipelineBuilder.inputAssemblyState = hri::GraphicsPipelineBuilder::initInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false);
	pipelineBuilder.viewport = hri::GraphicsPipelineBuilder::initDefaultViewport(static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height));
	pipelineBuilder.scissor = hri::GraphicsPipelineBuilder::initDefaultScissor(swapExtent.width, swapExtent.height);
	pipelineBuilder.rasterizationState = hri::GraphicsPipelineBuilder::initRasterizationState(false, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.multisampleState = hri::GraphicsPipelineBuilder::initMultisampleState(VK_SAMPLE_COUNT_1_BIT);
	pipelineBuilder.depthStencilState = hri::GraphicsPipelineBuilder::initDepthStencilState(false, false);
	pipelineBuilder.colorBlendState = hri::GraphicsPipelineBuilder::initColorBlendState({});
	pipelineBuilder.dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	pipelineBuilder.layout = m_layout;
	pipelineBuilder.renderPass = passResources->renderPass();
	pipelineBuilder.subpass = 0;

	m_pPSO = shaderDB.createPipeline("PresentPipeline", { "FullscreenQuadVert", "PresentFrag"}, pipelineBuilder);
}

PresentPass::~PresentPass()
{
	vkDestroyPipelineLayout(context.device, m_layout, nullptr);
}

void PresentPass::drawFrame(hri::ActiveFrame& frame)
{
	passResources->beginRenderPass(frame);

	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "Present Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	VkExtent2D swapExtent = context.swapchain.extent;
	VkViewport viewport = VkViewport{ 0.0f, 0.0f, static_cast<float>(swapExtent.width), static_cast<float>(swapExtent.height), hri::DefaultViewportMinDepth, hri::DefaultViewportMaxDepth };
	VkRect2D scissor = VkRect2D{ VkOffset2D{0, 0}, swapExtent };

	vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

	// TODO: Bind resource descriptors

	vkCmdBindPipeline(
		frame.commandBuffer,
		m_pPSO->bindPoint,
		m_pPSO->pipeline
	);

	vkCmdDraw(frame.commandBuffer, 3, 1, 0, 0);

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
	debug.cmdEndLabel(frame.commandBuffer);

	passResources->endRenderPass(frame);
}

// --- UI PASS ---

UIPass::UIPass(hri::RenderContext& ctx, VkDescriptorPool uiPool)
	:
	IRenderPass(ctx)
{
	hri::RenderPassBuilder passBuilder(ctx);
	passBuilder
		.addAttachment(
			ctx.swapFormat(), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		)
		.setAttachmentReference(hri::AttachmentType::Color, VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

	passResources = std::make_unique<hri::SwapchainPassResourceManager>(ctx, passBuilder.build());

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = ctx.instance;
	initInfo.PhysicalDevice = ctx.gpu;
	initInfo.Device = ctx.device;
	initInfo.QueueFamily = ctx.queues.graphicsQueue.family;
	initInfo.Queue = ctx.queues.graphicsQueue.handle;
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = uiPool;
	initInfo.Subpass = 0;
	initInfo.MinImageCount = ctx.swapchain.requested_min_image_count;
	initInfo.ImageCount = ctx.swapchain.image_count;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.RenderPass = passResources->renderPass();
	ImGui_ImplVulkan_Init(&initInfo);
}

UIPass::~UIPass()
{
	ImGui_ImplVulkan_Shutdown();
}

void UIPass::drawFrame(hri::ActiveFrame& frame)
{
	passResources->beginRenderPass(frame);

	debug.resetTimer();
	debug.cmdBeginLabel(frame.commandBuffer, "UI Pass");
	debug.cmdRecordStartTimestamp(frame.commandBuffer);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.commandBuffer);

	debug.cmdRecordEndTimestamp(frame.commandBuffer);
	debug.cmdEndLabel(frame.commandBuffer);

	passResources->endRenderPass(frame);
}
