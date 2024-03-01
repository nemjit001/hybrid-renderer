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
	VkPipeline pipeline,
	const RayTracingPipelineBuilder& pipelineBuilder
)
	:
	m_ctx(ctx),
	m_handleInfo(getShaderGroupHandleInfo(ctx))
{
	// Populate shader group count, indices & stride
	const uint32_t groupCount = static_cast<uint32_t>(pipelineBuilder.shaderGroups().size());
	getShaderGroupIndices(pipelineBuilder);
	getShaderGroupStrides();

	// Get shader group handles
	const uint32_t handleDataSize = groupCount * m_handleInfo.handleSize;
	std::vector<uint8_t> shaderHandles = std::vector<uint8_t>(handleDataSize);
	HRI_VK_CHECK(m_ctx.rayTracingDispatch.vkGetRayTracingShaderGroupHandles(
		m_ctx.renderContext.device,
		pipeline,
		0,
		groupCount,
		handleDataSize,
		shaderHandles.data()
	));

	// Calculate SBT sizes
	m_shaderGroupSizes[SBTShaderGroup::SGRayGen] = m_shaderGroupStrides[SBTShaderGroup::SGRayGen] * indexCount(SBTShaderGroup::SGRayGen);
	m_shaderGroupSizes[SBTShaderGroup::SGMiss] = m_shaderGroupStrides[SBTShaderGroup::SGMiss] * indexCount(SBTShaderGroup::SGMiss);
	m_shaderGroupSizes[SBTShaderGroup::SGHit] = m_shaderGroupStrides[SBTShaderGroup::SGHit] * indexCount(SBTShaderGroup::SGHit);
	m_shaderGroupSizes[SBTShaderGroup::SGCall] = m_shaderGroupStrides[SBTShaderGroup::SGCall] * indexCount(SBTShaderGroup::SGCall);

	// Allocate SBT datas & populate with shader group handles
	m_sbtData = {};
	for (uint32_t group = 0; group < m_shaderGroupIndices.size(); group++)
	{
		m_sbtData[group] = std::vector<uint8_t>(m_shaderGroupSizes[group]);
		uint8_t* pBufferData = m_sbtData[group].data();
		for (auto const& handleIdx : m_shaderGroupIndices[group])
		{
			memcpy(pBufferData, getShaderHandleOffset(shaderHandles.data(), handleIdx), m_handleInfo.handleSize);
			pBufferData += m_shaderGroupStrides[group];
		}
	}

	// Copy shader group handle data to buffers
	VkBufferUsageFlags sbtBufferUsage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	for (uint32_t group = 0; group < m_shaderGroupIndices.size(); group++)
	{
		if (m_shaderGroupIndices[group].size() > 0)
		{
			SBTShaderGroup shaderGroup = static_cast<SBTShaderGroup>(group);
			hri::BufferResource groupBuffer = hri::BufferResource(m_ctx.renderContext, m_shaderGroupSizes[group], sbtBufferUsage, true);
			groupBuffer.copyToBuffer(m_sbtData[group].data(), m_shaderGroupSizes[group]);

			auto& it = m_buffers.insert(std::make_pair(shaderGroup, std::move(groupBuffer)));
			assert(it.second == true);
		}
	}
}

VkDeviceAddress ShaderBindingTable::getGroupDeviceAddress(SBTShaderGroup group) const
{
	if (indexCount(group) == 0)
		return 0;

	return getDeviceAddress(m_ctx, m_buffers.at(group));
}

uint8_t* ShaderBindingTable::getShaderHandleOffset(uint8_t* pHandles, size_t idx) const
{
	return pHandles + idx * m_handleInfo.handleSize;
}

ShaderBindingTable::ShaderGroupHandleInfo ShaderBindingTable::getShaderGroupHandleInfo(RayTracingContext& ctx)
{
	VkPhysicalDeviceRayTracingPropertiesNV rtProps = VkPhysicalDeviceRayTracingPropertiesNV{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV };
	VkPhysicalDeviceProperties2 props = VkPhysicalDeviceProperties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	props.pNext = &rtProps;
	vkGetPhysicalDeviceProperties2(ctx.renderContext.gpu, &props);

	return ShaderGroupHandleInfo{
		rtProps.shaderGroupHandleSize,
		rtProps.shaderGroupBaseAlignment,
		static_cast<uint32_t>(HRI_ALIGNED_SIZE(rtProps.shaderGroupHandleSize, rtProps.shaderGroupBaseAlignment))
	};
}

const VkStridedDeviceAddressRegionKHR ShaderBindingTable::getRegion(SBTShaderGroup group, uint32_t offset) const
{
	return VkStridedDeviceAddressRegionKHR{ getGroupDeviceAddress(group) + offset * stride(group), stride(group), size(group)};
}

void ShaderBindingTable::getShaderGroupIndices(const RayTracingPipelineBuilder& pipelineBuilder)
{
	auto const& shaderStages = pipelineBuilder.shaderStages();
	auto const& shaderGroups = pipelineBuilder.shaderGroups();

	for (auto& indices : m_shaderGroupIndices)
		indices = {};

	for (uint32_t groupIdx = 0; groupIdx < shaderGroups.size(); groupIdx++)
	{
		auto const& rtShaderGroup = shaderGroups[groupIdx];

		if (rtShaderGroup.type == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR)
		{
			assert(rtShaderGroup.generalShader < shaderStages.size());
			auto const& generalShader = shaderStages[rtShaderGroup.generalShader];
			switch (generalShader.stage)
			{
			case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
				m_shaderGroupIndices[SBTShaderGroup::SGRayGen].push_back(groupIdx);
				break;
			case VK_SHADER_STAGE_MISS_BIT_KHR:
				m_shaderGroupIndices[SBTShaderGroup::SGMiss].push_back(groupIdx);
				break;
			case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
				m_shaderGroupIndices[SBTShaderGroup::SGHit].push_back(groupIdx);
				break;
			default:
				break;
			}
		}
		else // If type is not general, we are dealing with a hit group
		{
			m_shaderGroupIndices[SBTShaderGroup::SGHit].push_back(groupIdx);
		}
	}
}

void ShaderBindingTable::getShaderGroupStrides()
{
	for (size_t groupIdx = 0; groupIdx < m_shaderGroupIndices.size(); groupIdx++)
	{
		uint32_t stride = HRI_ALIGNED_SIZE(m_handleInfo.alignedHandleSize, m_handleInfo.baseAlignment);
		m_shaderGroupStrides[groupIdx] = stride;
	}

	// ensure sg raygen is aligned on group base alignment
	m_shaderGroupStrides[SBTShaderGroup::SGRayGen] = HRI_ALIGNED_SIZE(
		m_shaderGroupStrides[SBTShaderGroup::SGRayGen],
		m_handleInfo.baseAlignment
	);
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
