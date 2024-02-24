#include "detail/raytracing.h"

#include <hybrid_renderer.h>
#include <vector>

#include "demo.h"
#include "detail/dispatch.h"

RayTracingContext::RayTracingContext(RayTracingContext&& other)
	:
	renderContext(other.renderContext),
	deferredHostDispatch(other.deferredHostDispatch),
	accelStructDispatch(other.accelStructDispatch),
	rayTracingDispatch(other.rayTracingDispatch)
{
	//
}

RayTracingContext& RayTracingContext::operator=(RayTracingContext&& other)
{
	if (this == &other)
	{
		return *this;
	}

	renderContext = std::move(other.renderContext);
	deferredHostDispatch = other.deferredHostDispatch;
	accelStructDispatch = other.accelStructDispatch;
	rayTracingDispatch = other.rayTracingDispatch;

	return *this;
}

RayTracingPipelineBuilder::RayTracingPipelineBuilder(RayTracingContext& ctx)
	:
	m_ctx(ctx)
{
	//
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::addShaderStage(
	VkShaderStageFlagBits stage,
	VkShaderModule module,
	const char* entryPoint
)
{
	VkPipelineShaderStageCreateInfo shaderStage = VkPipelineShaderStageCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	shaderStage.stage = stage;
	shaderStage.module = module;
	shaderStage.pName = entryPoint;
	shaderStage.pSpecializationInfo = nullptr;
	m_shaderStages.push_back(shaderStage);

	return *this;
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::addRayTracingShaderGroup(
	VkRayTracingShaderGroupTypeKHR type,
	uint32_t generalShader,
	uint32_t closestHitShader,
	uint32_t anyHitShader,
	uint32_t intersectionShader
)
{
	VkRayTracingShaderGroupCreateInfoKHR shaderGroup = VkRayTracingShaderGroupCreateInfoKHR{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
	shaderGroup.type = type;
	shaderGroup.generalShader = generalShader;
	shaderGroup.closestHitShader = closestHitShader;
	shaderGroup.anyHitShader = anyHitShader;
	shaderGroup.intersectionShader = intersectionShader;
	shaderGroup.pShaderGroupCaptureReplayHandle = nullptr; // TODO: check what this does
	m_shaderGroups.push_back(shaderGroup);

	return *this;
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::setMaxRecursionDepth(uint32_t depth)
{
	m_maxRecursionDepth = depth;

	return *this;
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::addDynamicStates(const std::vector<VkDynamicState>& dynamicStates)
{
	m_dynamicStates.insert(m_dynamicStates.end(), dynamicStates.begin(), dynamicStates.end());

	return *this;
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::setLayout(VkPipelineLayout layout)
{
	m_layout = layout;

	return *this;
}

RayTracingPipelineBuilder& RayTracingPipelineBuilder::setCreateFlags(VkPipelineCreateFlags flags)
{
	m_flags = flags;

	return *this;
}

VkPipeline RayTracingPipelineBuilder::build(VkPipelineCache cache, VkDeferredOperationKHR deferredOperation)
{
	VkPipelineDynamicStateCreateInfo dynamicState = VkPipelineDynamicStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = static_cast<uint32>(m_dynamicStates.size());
	dynamicState.pDynamicStates = m_dynamicStates.data();

	VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = VkRayTracingPipelineCreateInfoKHR{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	pipelineCreateInfo.flags = m_flags;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
	pipelineCreateInfo.pStages = m_shaderStages.data();
	pipelineCreateInfo.groupCount = static_cast<uint32_t>(m_shaderGroups.size());
	pipelineCreateInfo.pGroups = m_shaderGroups.data();
	pipelineCreateInfo.maxPipelineRayRecursionDepth = m_maxRecursionDepth;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.layout = m_layout;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = 0;

	VkPipeline pipeline = VK_NULL_HANDLE;
	HRI_VK_CHECK(m_ctx.rayTracingDispatch.vkCmdCreateRaytracingPipelines(
		m_ctx.renderContext.device,
		VK_NULL_HANDLE,
		cache,
		1,
		&pipelineCreateInfo,
		nullptr,
		&pipeline
	));

	return pipeline;
}

AccelerationStructure::AccelerationStructure(RayTracingContext& ctx, VkAccelerationStructureTypeKHR type, size_t size)
	:
	m_ctx(ctx),
	buffer(m_ctx.renderContext, size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR)
{
	VkAccelerationStructureCreateInfoKHR createInfo = VkAccelerationStructureCreateInfoKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.createFlags = 0;
    createInfo.buffer = buffer.buffer;
    createInfo.offset = 0;
    createInfo.size = buffer.bufferSize;
    createInfo.type = type;
    createInfo.deviceAddress = 0;

	VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
	HRI_VK_CHECK(m_ctx.accelStructDispatch.vkCreateAccelerationStructure(
		m_ctx.renderContext.device,
		&createInfo,
		nullptr,
		&accelerationStructure
	));
}

AccelerationStructure::~AccelerationStructure()
{
	release();
}

AccelerationStructure::AccelerationStructure(AccelerationStructure&& other) noexcept
	:
	m_ctx(other.m_ctx),
	buffer(std::move(other.buffer)),
	accelerationStructure(other.accelerationStructure)
{
	other.accelerationStructure = VK_NULL_HANDLE;
}

AccelerationStructure& AccelerationStructure::operator=(AccelerationStructure&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	release();	
	m_ctx = std::move(other.m_ctx);
	buffer = std::move(other.buffer);
	accelerationStructure = other.accelerationStructure;

	other.accelerationStructure = VK_NULL_HANDLE;

	return *this;
}

void AccelerationStructure::release()
{
	m_ctx.accelStructDispatch.vkDestroyAccelerationStructure(m_ctx.renderContext.device, accelerationStructure, nullptr);
}
