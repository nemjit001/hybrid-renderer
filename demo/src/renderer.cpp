#include "renderer.h"

#include <hybrid_renderer.h>
#include <memory>

#include "subsystems.h"

Renderer::Renderer(hri::RenderContext& ctx, hri::Camera& camera)
	:
	m_context(ctx),
	m_renderCore(&ctx),
	m_shaderDatabase(&ctx),
	m_subsystemManager(&ctx),
	m_descriptorSetAllocator(&ctx),
	m_worldCam(camera)
{
	initShaderDB();
	initRenderPasses();
	initSharedResources();
	initGlobalDescriptorSets();
	initRenderSubsystems();

	m_renderCore.setOnSwapchainInvalidateCallback([this](const vkb::Swapchain& _swapchain) {
		recreateSwapDependentResources();
	});

	VkDescriptorBufferInfo worldCamBufferInfo = VkDescriptorBufferInfo{};
	worldCamBufferInfo.buffer = m_worldCameraUBO.buffer;
	worldCamBufferInfo.offset = 0;
	worldCamBufferInfo.range = VK_WHOLE_SIZE;

	(*m_sceneDataSet)
		.writeBuffer(0, &worldCamBufferInfo)
		.flush();

	VkDescriptorImageInfo renderResultImageInfo = VkDescriptorImageInfo{};
	renderResultImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	renderResultImageInfo.imageView = m_gbufferLayoutPassManager->getAttachmentResource(0).view;
	renderResultImageInfo.sampler = m_renderResultLinearSampler->sampler();

	(*m_presentInputSet)
		.writeImage(0, &renderResultImageInfo)
		.flush();
}

Renderer::~Renderer()
{
	m_renderCore.awaitFrameFinished();

	hri::BufferResource::destroy(&m_context, m_worldCameraUBO);
}

void Renderer::drawFrame()
{
	hri::CameraShaderData worldCamShaderData = m_worldCam.getShaderData();
	m_worldCameraUBO.copyToBuffer(&m_context, &worldCamShaderData, sizeof(hri::CameraShaderData));

	m_renderCore.startFrame();

	hri::ActiveFrame frame = m_renderCore.getActiveFrame();
	frame.beginCommands();

	// Bind scene data descriptor sets
	VkDescriptorSet sceneDataInputSets[] = { m_sceneDataSet->descriptorSet() };
	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_gbufferLayoutSubsystem->pipelineLayout(),
		0, 1, sceneDataInputSets,
		0, nullptr
	);

	m_gbufferLayoutPassManager->beginRenderPass(frame);
	m_subsystemManager.recordSubsystem("GBufferLayoutSystem", frame);
	m_gbufferLayoutPassManager->endRenderPass(frame);

	frame.imageMemoryBarrier(
		m_gbufferLayoutPassManager->getAttachmentResource(0).image,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
	);

	// Bind presentation input descriptor sets
	VkDescriptorSet presentInputSets[] = { m_presentInputSet->descriptorSet() };
	vkCmdBindDescriptorSets(
		frame.commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		m_presentSubsystem->pipelineLayout(),
		0, 1, presentInputSets,
		0, nullptr
	);
	
	m_presentPassManager->beginRenderPass(frame);
	m_subsystemManager.recordSubsystem("PresentationSystem", frame);
	m_subsystemManager.recordSubsystem("UISystem", frame);	// Must be drawn after present as overlay
	m_presentPassManager->endRenderPass(frame);

	frame.endCommands();
	m_renderCore.endFrame();
}

void Renderer::initShaderDB()
{
	m_shaderDatabase.registerShader("PresentVert", hri::Shader::loadFile(&m_context, "./shaders/present.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
	m_shaderDatabase.registerShader("StaticVert", hri::Shader::loadFile(&m_context, "./shaders/static.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));

	m_shaderDatabase.registerShader("GBufferLayoutFrag", hri::Shader::loadFile(&m_context, "./shaders/gbuffer_layout.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
	m_shaderDatabase.registerShader("PresentFrag", hri::Shader::loadFile(&m_context, "./shaders/present.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
}

void Renderer::initRenderPasses()
{
	// GBuffer Layout pass
	{
		hri::RenderPassBuilder gbufferLayoutPassBuilder = hri::RenderPassBuilder(&m_context)
			.addAttachment(
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(
				VK_FORMAT_R8G8B8A8_SNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(
				VK_FORMAT_R8G8B8A8_SNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.addAttachment(
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			)
			.setAttachmentReference(
				hri::AttachmentType::DepthStencil,
				VkAttachmentReference{ 3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
			);

		const std::vector<hri::RenderAttachmentConfig> gbufferAttachmentConfigs = {
			hri::RenderAttachmentConfig{ // Albedo target
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // wPos target
				VK_FORMAT_R8G8B8A8_SNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Normal target
				VK_FORMAT_R8G8B8A8_SNORM,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT
			},
			hri::RenderAttachmentConfig{ // Depth Stencil target
				VK_FORMAT_D24_UNORM_S8_UINT,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
				| VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_IMAGE_ASPECT_DEPTH_BIT
				| VK_IMAGE_ASPECT_STENCIL_BIT
			},
		};

		m_gbufferLayoutPassManager = std::make_unique<hri::RenderPassResourceManager>(
			&m_context,
			gbufferLayoutPassBuilder.build(),
			gbufferAttachmentConfigs
		);
	}

	// Present pass
	{
		hri::RenderPassBuilder presentPassBuilder = hri::RenderPassBuilder(&m_context)
			.addAttachment(
				m_context.swapFormat(),
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_ATTACHMENT_LOAD_OP_CLEAR,
				VK_ATTACHMENT_STORE_OP_STORE
			)
			.setAttachmentReference(
				hri::AttachmentType::Color,
				VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
			);

		m_presentPassManager = std::make_unique<hri::SwapchainPassResourceManager>(
			&m_context,
			presentPassBuilder.build()
		);
	}

	m_gbufferLayoutPassManager->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(1, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(2, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(3, VkClearValue{ { 1.0f, 0x00 } });

	m_presentPassManager->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 1.0f } });
}

void Renderer::initSharedResources()
{
	// init shared samplers
	m_renderResultLinearSampler = std::make_unique<hri::ImageSampler>(
		&m_context,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR
	);

	// init shared resources
	m_worldCameraUBO = hri::BufferResource::init(
		&m_context,
		sizeof(hri::CameraShaderData),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		true	// FIXME: make this buffer device local
	);
}

void Renderer::initGlobalDescriptorSets()
{
	hri::DescriptorSetLayoutBuilder sceneDataSetBuilder = hri::DescriptorSetLayoutBuilder(&m_context)
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

	hri::DescriptorSetLayoutBuilder presentInputSetBuilder = hri::DescriptorSetLayoutBuilder(&m_context)
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_sceneDataSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(sceneDataSetBuilder.build()));
	m_sceneDataSet = std::make_unique<hri::DescriptorSetManager>(&m_context, &m_descriptorSetAllocator, *m_sceneDataSetLayout);

	m_presentInputSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(presentInputSetBuilder.build()));
	m_presentInputSet = std::make_unique<hri::DescriptorSetManager>(&m_context, &m_descriptorSetAllocator, *m_presentInputSetLayout);
}

void Renderer::initRenderSubsystems()
{
	m_gbufferLayoutSubsystem = std::make_unique<GBufferLayoutSubsystem>(
		&m_context,
		&m_descriptorSetAllocator,
		&m_shaderDatabase,
		m_gbufferLayoutPassManager->renderPass(),
		*m_sceneDataSetLayout
	);

	m_uiSubsystem = std::make_unique<UISubsystem>(
		&m_context,
		&m_descriptorSetAllocator,
		&m_shaderDatabase,
		m_presentPassManager->renderPass()
	);

	m_presentSubsystem = std::make_unique<PresentationSubsystem>(
		&m_context,
		&m_descriptorSetAllocator,
		&m_shaderDatabase,
		m_presentPassManager->renderPass(),
		*m_presentInputSetLayout
	);

	m_subsystemManager.registerSubsystem("GBufferLayoutSystem", m_gbufferLayoutSubsystem.get());
	m_subsystemManager.registerSubsystem("UISystem", m_uiSubsystem.get());
	m_subsystemManager.registerSubsystem("PresentationSystem", m_presentSubsystem.get());
}

void Renderer::recreateSwapDependentResources()
{
	m_gbufferLayoutPassManager->recreateResources();
	m_presentPassManager->recreateResources();

	// render result descriptor should be updated to account for new image view handles
	VkDescriptorImageInfo renderResultImageInfo = VkDescriptorImageInfo{};
	renderResultImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	renderResultImageInfo.imageView = m_gbufferLayoutPassManager->getAttachmentResource(0).view;
	renderResultImageInfo.sampler = m_renderResultLinearSampler->sampler();

	(*m_presentInputSet)
		.writeImage(0, &renderResultImageInfo)
		.flush();
}
