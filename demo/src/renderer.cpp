#include "renderer.h"

#include <hybrid_renderer.h>
#include <memory>

#include "subsystems.h"

Renderer::Renderer(hri::RenderContext& ctx, hri::Camera& camera, hri::Scene& scene)
	:
	m_context(ctx),
	m_renderCore(&ctx),
	m_shaderDatabase(&ctx),
	m_subsystemManager(&ctx),
	m_descriptorSetAllocator(&ctx),
	m_camera(camera),
	m_activeScene(scene)
{
	initShaderDB();
	initRenderPasses();
	initSharedResources();
	initGlobalDescriptorSets();
	initRenderSubsystems();
	initRendererFrameData();

	m_renderCore.setOnSwapchainInvalidateCallback([this](const vkb::Swapchain& _swapchain) {
		recreateSwapDependentResources();
	});
}

Renderer::~Renderer()
{
	m_renderCore.awaitFrameFinished();

	for (auto& frame : m_frames)
	{
		hri::BufferResource::destroy(&m_context, frame.cameraUBO);
	}
}

void Renderer::setActiveScene(hri::Scene& scene)
{
	m_activeScene = scene;
}

void Renderer::drawFrame()
{
	m_renderCore.startFrame();

	hri::ActiveFrame frame = m_renderCore.getActiveFrame();
	prepareFrameResources(frame);

	frame.beginCommands();

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
	
	m_swapchainPassManager->beginRenderPass(frame);
	m_subsystemManager.recordSubsystem("PresentationSystem", frame);
	m_subsystemManager.recordSubsystem("UISystem", frame);	// Must be drawn after present as overlay
	m_swapchainPassManager->endRenderPass(frame);

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

		m_gbufferLayoutPassManager = std::unique_ptr<hri::RenderPassResourceManager>(new hri::RenderPassResourceManager(
			&m_context,
			gbufferLayoutPassBuilder.build(),
			gbufferAttachmentConfigs
		));
	}

	// Swapchain pass pass
	{
		hri::RenderPassBuilder swapchainPassBuilder = hri::RenderPassBuilder(&m_context)
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

		m_swapchainPassManager = std::unique_ptr<hri::SwapchainPassResourceManager>(new hri::SwapchainPassResourceManager(
			&m_context,
			swapchainPassBuilder.build()
		));
	}

	m_gbufferLayoutPassManager->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(1, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(2, VkClearValue{ { 0.0f, 0.0f, 0.0f, 0.0f } });
	m_gbufferLayoutPassManager->setClearValue(3, VkClearValue{ { 1.0f, 0x00 } });

	m_swapchainPassManager->setClearValue(0, VkClearValue{ { 0.0f, 0.0f, 0.0f, 1.0f } });
}

void Renderer::initSharedResources()
{
	// init shared samplers
	m_renderResultLinearSampler = std::unique_ptr<hri::ImageSampler>(new hri::ImageSampler(
		&m_context,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR
	));
}

void Renderer::initGlobalDescriptorSets()
{
	hri::DescriptorSetLayoutBuilder sceneDataSetBuilder = hri::DescriptorSetLayoutBuilder(&m_context)
		.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

	m_sceneDataSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(sceneDataSetBuilder.build()));

	hri::DescriptorSetLayoutBuilder presentInputSetBuilder = hri::DescriptorSetLayoutBuilder(&m_context)
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_presentInputSetLayout = std::unique_ptr<hri::DescriptorSetLayout>(new hri::DescriptorSetLayout(presentInputSetBuilder.build()));
}

void Renderer::initRenderSubsystems()
{
	m_gbufferLayoutSubsystem = std::unique_ptr<GBufferLayoutSubsystem>(new GBufferLayoutSubsystem(
		&m_context,
		&m_shaderDatabase,
		m_gbufferLayoutPassManager->renderPass(),
		m_sceneDataSetLayout->setLayout
	));

	m_uiSubsystem = std::unique_ptr<UISubsystem>(new UISubsystem(
		&m_context,
		m_swapchainPassManager->renderPass(),
		m_descriptorSetAllocator.fixedPool()
	));

	m_presentSubsystem = std::unique_ptr<PresentationSubsystem>(new PresentationSubsystem(
		&m_context,
		&m_shaderDatabase,
		m_swapchainPassManager->renderPass(),
		m_presentInputSetLayout->setLayout
	));

	m_subsystemManager.registerSubsystem("GBufferLayoutSystem", m_gbufferLayoutSubsystem.get());
	m_subsystemManager.registerSubsystem("UISystem", m_uiSubsystem.get());
	m_subsystemManager.registerSubsystem("PresentationSystem", m_presentSubsystem.get());
}

void Renderer::initRendererFrameData()
{
	for (size_t i = 0; i < hri::RenderCore::framesInFlight(); i++)
	{
		RendererFrameData& frame = m_frames[i];
		frame.cameraUBO = hri::BufferResource::init(
			&m_context,
			sizeof(hri::CameraShaderData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			true	// FIXME: make buffer device local & prepare resource before frame execution.
		);

		frame.presentInputSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			&m_context,
			&m_descriptorSetAllocator,
			*m_presentInputSetLayout
		));

		frame.sceneDataSet = std::unique_ptr<hri::DescriptorSetManager>(new hri::DescriptorSetManager(
			&m_context,
			&m_descriptorSetAllocator,
			*m_sceneDataSetLayout
		));
	}
}

void Renderer::recreateSwapDependentResources()
{
	m_gbufferLayoutPassManager->recreateResources();
	m_swapchainPassManager->recreateResources();
}

void Renderer::prepareFrameResources(const hri::ActiveFrame& frame)
{
	RendererFrameData& rendererFrameData = m_frames[frame.currentFrameIndex];

	// Update UBOs & scene renderables
	hri::CameraShaderData cameraData = m_camera.getShaderData();
	rendererFrameData.cameraUBO.copyToBuffer(&m_context, &cameraData, sizeof(hri::CameraShaderData));
	rendererFrameData.renderableScene = m_activeScene.generateRenderableScene(m_camera);

	// Update subsystem frame info
	m_gbufferLayoutSubsystem->updateFrameInfo(GBufferLayoutFrameInfo{
		rendererFrameData.sceneDataSet->set,
		&rendererFrameData.renderableScene,
	});

	m_presentSubsystem->updateFrameInfo(PresentFrameInfo{
		rendererFrameData.presentInputSet->set,
	});

	// Update per frame descriptors
	VkDescriptorBufferInfo cameraUBOInfo = VkDescriptorBufferInfo{};
	cameraUBOInfo.buffer = rendererFrameData.cameraUBO.buffer;
	cameraUBOInfo.offset = 0;
	cameraUBOInfo.range = rendererFrameData.cameraUBO.bufferSize;

	(*rendererFrameData.sceneDataSet)
		.writeBuffer(0, &cameraUBOInfo)
		.flush();

	VkDescriptorImageInfo gbufferAlbedoResult = VkDescriptorImageInfo{};
	gbufferAlbedoResult.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	gbufferAlbedoResult.imageView = m_gbufferLayoutPassManager->getAttachmentResource(0).view;
	gbufferAlbedoResult.sampler = m_renderResultLinearSampler->sampler;

	(*rendererFrameData.presentInputSet)
		.writeImage(0, &gbufferAlbedoResult)
		.flush();
}
