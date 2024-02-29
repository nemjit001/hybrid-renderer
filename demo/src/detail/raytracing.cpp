#include "detail/raytracing.h"

#include <hybrid_renderer.h>
#include <vector>

#include "demo.h"
#include "detail/dispatch.h"

using namespace raytracing;

VkDeviceAddress raytracing::getDeviceAddress(const RayTracingContext& ctx, const hri::BufferResource& resource)
{
	VkBufferDeviceAddressInfo addressInfo = resource.deviceAddressInfo();
	return vkGetBufferDeviceAddress(ctx.renderContext.device, &addressInfo);
}

VkTransformMatrixKHR raytracing::toTransformMatrix(const hri::Float4x4& mat)
{
	// Convert from column major layout to row major layout.
	const hri::Float4x4 out = transpose(mat);
	VkTransformMatrixKHR vkMat34 = VkTransformMatrixKHR{};
	memcpy(&vkMat34, &out, sizeof(VkTransformMatrixKHR));
	return vkMat34;
}

RayTracingContext::RayTracingContext(RayTracingContext&& other) noexcept
	:
	renderContext(other.renderContext),
	deferredHostDispatch(other.deferredHostDispatch),
	accelStructDispatch(other.accelStructDispatch),
	rayTracingDispatch(other.rayTracingDispatch)
{
	//
}

RayTracingContext& RayTracingContext::operator=(RayTracingContext&& other) noexcept
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

ShaderBindingTable::ShaderBindingTable(
	RayTracingContext& ctx,
	VkStridedDeviceAddressRegionKHR rayGenRegion,
	VkStridedDeviceAddressRegionKHR rayMissRegion,
	VkStridedDeviceAddressRegionKHR rayHitRegion,
	VkStridedDeviceAddressRegionKHR rayCallRegion
)
	:
	m_ctx(ctx),
	m_handleInfo(ShaderBindingTable::getShaderGroupHandleInfo(ctx)),
	m_SBTSize(rayGenRegion.size + rayMissRegion.size + rayHitRegion.size + rayCallRegion.size),
	m_SBTBuffer(
		m_ctx.renderContext,
		m_SBTSize,
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR
		| VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		true
	),
	regions{ rayGenRegion, rayMissRegion, rayHitRegion, rayCallRegion }
{
	//
}

ShaderBindingTable::ShaderGroupHandleInfo ShaderBindingTable::getShaderGroupHandleInfo(RayTracingContext& ctx)
{
	VkPhysicalDeviceRayTracingPropertiesNV rtProperties = VkPhysicalDeviceRayTracingPropertiesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV };
	VkPhysicalDeviceProperties2 properties = VkPhysicalDeviceProperties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR };
	properties.pNext = &rtProperties;

	vkGetPhysicalDeviceProperties2(ctx.renderContext.gpu, &properties);
	size_t handleSize = rtProperties.shaderGroupHandleSize;
	size_t baseAlignment = rtProperties.shaderGroupBaseAlignment;

	return ShaderGroupHandleInfo{
		handleSize,
		baseAlignment,
		HRI_ALIGNED_SIZE(handleSize, baseAlignment),
	};
}

void ShaderBindingTable::populateSBT(
	const std::vector<uint8_t>& shaderGroupHandles,
	size_t missHandleCount,
	size_t hitHandleCount,
	size_t callHandleCount
)
{
	// TODO: check SBT is not written to out of bounds
	VkDeviceAddress SBTAdress = raytracing::getDeviceAddress(m_ctx, m_SBTBuffer);

	// Set region device addresses
	regions[SBTRegion::SBTRayGen].deviceAddress = SBTAdress + getAddressRegionOffset(SBTRegion::SBTRayGen);
	regions[SBTRegion::SBTMiss].deviceAddress = SBTAdress + getAddressRegionOffset(SBTRegion::SBTMiss);
	regions[SBTRegion::SBTHit].deviceAddress = SBTAdress + getAddressRegionOffset(SBTRegion::SBTHit);
	if (callHandleCount > 0) regions[SBTRegion::SBTCall].deviceAddress = SBTAdress + getAddressRegionOffset(SBTRegion::SBTCall);

	// Retrieve buffer pointers
	size_t shaderHandleIdx = 0;
	const uint8_t* pHandles = shaderGroupHandles.data();
	uint8_t* pSBTData = reinterpret_cast<uint8_t*>(m_SBTBuffer.map());
	uint8_t* pTargetData = nullptr;

	// Copy ray gen
	pTargetData = pSBTData + getAddressRegionOffset(SBTRegion::SBTRayGen);
	memcpy(pTargetData, getShaderGroupHandleOffset(pHandles, shaderHandleIdx), m_handleInfo.handleSize);
	shaderHandleIdx++;

	// Copy ray miss
	pTargetData = pSBTData + getAddressRegionOffset(SBTRegion::SBTMiss);
	for (size_t i = 0; i < missHandleCount; i++, shaderHandleIdx++)
	{
		memcpy(pTargetData, getShaderGroupHandleOffset(pHandles, shaderHandleIdx), m_handleInfo.handleSize);
		pTargetData += regions[SBTRegion::SBTMiss].stride;
	}

	// Copy ray hit
	pTargetData = pSBTData + getAddressRegionOffset(SBTRegion::SBTHit);
	for (size_t i = 0; i < hitHandleCount; i++, shaderHandleIdx++)
	{
		memcpy(pTargetData, getShaderGroupHandleOffset(pHandles, shaderHandleIdx), m_handleInfo.handleSize);
		pTargetData += regions[SBTRegion::SBTHit].stride;
	}

	// Copy ray call
	pTargetData = pSBTData + getAddressRegionOffset(SBTRegion::SBTCall);
	for (size_t i = 0; i < callHandleCount; i++, shaderHandleIdx++)
	{
		memcpy(pTargetData, getShaderGroupHandleOffset(pHandles, shaderHandleIdx), m_handleInfo.handleSize);
		pTargetData += regions[SBTRegion::SBTCall].stride;
	}

	m_SBTBuffer.unmap();
}

size_t ShaderBindingTable::getAddressRegionOffset(SBTRegion region) const
{
	switch (region)
	{
	case SBTRegion::SBTRayGen:
		return 0;
	case SBTRegion::SBTMiss:
		return regions[SBTRayGen].size;
	case SBTRegion::SBTHit:
		return regions[SBTRayGen].size + regions[SBTMiss].size;
	case SBTRegion::SBTCall:
		return regions[SBTRayGen].size + regions[SBTMiss].size + regions[SBTHit].size;
	default:
		return 0;
	}
}

 const uint8_t* ShaderBindingTable::getShaderGroupHandleOffset(const uint8_t* pHandles, size_t index) const
{
	assert(pHandles != nullptr);
	return pHandles + index * m_handleInfo.handleSize;
}

AccelerationStructure::AccelerationStructure(RayTracingContext& ctx, VkAccelerationStructureTypeKHR type, size_t size)
	:
	m_ctx(ctx),
	buffer(ctx.renderContext, size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
{
	VkAccelerationStructureCreateInfoKHR createInfo = VkAccelerationStructureCreateInfoKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.createFlags = 0;
    createInfo.buffer = buffer.buffer;
    createInfo.offset = 0;
    createInfo.size = buffer.bufferSize;
    createInfo.type = type;
    createInfo.deviceAddress = 0;

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

ASBuilder::ASBuilder(RayTracingContext& ctx)
	:
	m_ctx(ctx)
{
	//
}

ASBuilder::ASInput ASBuilder::objectToGeometry(
	const hri::Mesh& mesh,
	VkBuildAccelerationStructureFlagsKHR buildFlags,
	VkGeometryFlagsKHR geometryFlags
) const
{
	const VkDeviceAddress vertexAddress = raytracing::getDeviceAddress(m_ctx, mesh.vertexBuffer);
	const VkDeviceAddress indexAddress = raytracing::getDeviceAddress(m_ctx, mesh.indexBuffer);
	const uint32_t maxPrimitiveCount = mesh.indexCount / 3;
	const uint32_t maxVertexIndex = mesh.vertexCount - 1;

	VkAccelerationStructureGeometryTrianglesDataKHR triangles = VkAccelerationStructureGeometryTrianglesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vertexAddress;
	triangles.vertexStride = sizeof(hri::Vertex);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	triangles.transformData = VkDeviceOrHostAddressConstKHR{};	// Use identity transform
	triangles.maxVertex = maxVertexIndex;

	VkAccelerationStructureGeometryKHR geometry = VkAccelerationStructureGeometryKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.flags = geometryFlags;
	geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR buildOffset = VkAccelerationStructureBuildRangeInfoKHR{};
	buildOffset.firstVertex = 0;
	buildOffset.primitiveCount = maxPrimitiveCount;
	buildOffset.primitiveOffset = offsetof(hri::Vertex, position);
	buildOffset.transformOffset = 0;	// unused

	ASInput asInput = ASInput{};
	asInput.buildFlags = buildFlags;
	asInput.geometry.push_back(geometry);
	asInput.buildOffsets.push_back(buildOffset);

	return asInput;
}

ASBuilder::ASInput ASBuilder::instancesToGeometry(
	const hri::BufferResource& instanceBuffer,
	size_t instanceCount,
	bool arrayOfPointers,
	VkBuildAccelerationStructureFlagsKHR buildFlags,
	VkGeometryFlagsKHR geometryFlags
) const
{
	VkAccelerationStructureGeometryInstancesDataKHR instanceGeometry = VkAccelerationStructureGeometryInstancesDataKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	instanceGeometry.arrayOfPointers = arrayOfPointers;
	instanceGeometry.data.deviceAddress = raytracing::getDeviceAddress(m_ctx, instanceBuffer);

	VkAccelerationStructureGeometryKHR geometry = VkAccelerationStructureGeometryKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.flags = geometryFlags;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances = instanceGeometry;

	VkAccelerationStructureBuildRangeInfoKHR buildOffset = VkAccelerationStructureBuildRangeInfoKHR{};
	buildOffset.firstVertex = 0;
	buildOffset.primitiveCount = static_cast<uint32_t>(instanceCount);
	buildOffset.primitiveOffset = 0;
	buildOffset.transformOffset = 0;

	ASInput asInput = ASInput{};
	asInput.buildFlags = buildFlags;
	asInput.geometry.push_back(geometry);
	asInput.buildOffsets.push_back(buildOffset);

	return asInput;
}

ASBuilder::ASBuildInfo ASBuilder::generateASBuildInfo(
	const ASInput& input,
	ASSizeInfo& sizeInfo,
	VkAccelerationStructureTypeKHR asType,
	VkBuildAccelerationStructureModeKHR buildMode,
	VkBuildAccelerationStructureFlagsKHR flags
) const
{
	ASBuildInfo buildInfo = ASBuildInfo{};

	// Geometry info
	buildInfo.geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.geometryInfo.flags = input.buildFlags | flags;
	buildInfo.geometryInfo.type = asType;
	buildInfo.geometryInfo.mode = buildMode;
	buildInfo.geometryInfo.geometryCount = static_cast<uint32_t>(input.geometry.size());
	buildInfo.geometryInfo.pGeometries = input.geometry.data();

	// Offset info
	buildInfo.buildRanges = input.buildOffsets.data();

	// Build size
	buildInfo.buildSizes = VkAccelerationStructureBuildSizesInfoKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

	// Max primitive info
	std::vector<uint32_t> maxPrimitiveCounts = {}; maxPrimitiveCounts.reserve(input.buildOffsets.size());
	for (auto const& offset : input.buildOffsets)
		maxPrimitiveCounts.push_back(offset.primitiveCount);

	// Retrieve build size infos
	m_ctx.accelStructDispatch.vkGetAccelerationStructureBuildSizes(
		m_ctx.renderContext.device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo.geometryInfo,
		maxPrimitiveCounts.data(),
		&buildInfo.buildSizes
	);

	sizeInfo.totalAccelerationStructureSize += buildInfo.buildSizes.accelerationStructureSize;
	sizeInfo.maxBuildScratchBufferSize = hri::max(buildInfo.buildSizes.buildScratchSize, sizeInfo.maxBuildScratchBufferSize);
	sizeInfo.maxUpdateScratchBufferSize = hri::max(buildInfo.buildSizes.updateScratchSize, sizeInfo.maxUpdateScratchBufferSize);

	return buildInfo;
}

std::vector<ASBuilder::ASBuildInfo> ASBuilder::generateASBuildInfo(
	const std::vector<ASInput>& inputs,
	ASSizeInfo& sizeInfo,
	VkAccelerationStructureTypeKHR asType,
	VkBuildAccelerationStructureModeKHR buildMode,
	VkBuildAccelerationStructureFlagsKHR flags
) const
{
	const uint32_t blasCount = static_cast<uint32_t>(inputs.size());

	sizeInfo = ASSizeInfo{};
	std::vector<ASBuildInfo> buildInfos = std::vector<ASBuildInfo>(blasCount);

	for (uint32_t i = 0; i < blasCount; i++)
	{
		const ASInput& input = inputs[i];
		ASBuildInfo& buildInfo = buildInfos[i];

		buildInfo = generateASBuildInfo(input, sizeInfo, asType, buildMode, flags);
	}

	return buildInfos;
}

void ASBuilder::cmdBuildAccelerationStructure(
	VkCommandBuffer commandBuffer,
	const ASBuildInfo& buildInfo,
	VkAccelerationStructureKHR structure,
	VkDeviceAddress scratchDataAddress
) const
{
	// Extend build info
	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = buildInfo.geometryInfo;
	buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	buildGeometryInfo.dstAccelerationStructure = structure;
	buildGeometryInfo.scratchData.deviceAddress = scratchDataAddress;

	// Build acceleration structure
	m_ctx.accelStructDispatch.vkCmdBuildAccelerationStructures(
		commandBuffer,
		1,
		&buildGeometryInfo,
		&buildInfo.buildRanges
	);
}

void ASBuilder::cmdBuildAccelerationStructures(
	VkCommandBuffer commandBuffer,
	const std::vector<ASBuildInfo>& buildInfos,
	std::vector<VkAccelerationStructureKHR>& structures,
	VkDeviceAddress scratchDataAddress
) const
{
	uint32_t blasCount = static_cast<uint32_t>(buildInfos.size());
	assert(buildInfos.size() == structures.size());

	// Simple unbatched command recording
	for (uint32_t blasIdx = 0; blasIdx < blasCount; blasIdx++)
	{
		cmdBuildAccelerationStructure(
			commandBuffer,
			buildInfos[blasIdx],
			structures[blasIdx],
			scratchDataAddress
		);

		VkMemoryBarrier barrier = VkMemoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0,
			1, &barrier,
			0, nullptr,
			0, nullptr
		);
	}
}
