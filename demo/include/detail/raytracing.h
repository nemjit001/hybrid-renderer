#pragma once

#include <hybrid_renderer.h>
#include <vector>

#include "demo.h"
#include "detail/dispatch.h"

/// @brief A Ray Tracing Context uses a render context to initialize dispatch tables required for ray tracing.
///		These dispatch tables need to be used for all extension functions.
struct RayTracingContext
{
	RayTracingContext() = delete;
	RayTracingContext(hri::RenderContext& ctx) : renderContext(ctx) {}

	RayTracingContext(RayTracingContext&) = delete;
	RayTracingContext& operator=(RayTracingContext&) = delete;

	RayTracingContext(RayTracingContext&& other);
	RayTracingContext& operator=(RayTracingContext&& other);

	hri::RenderContext& renderContext;
	DeferredHostOperationsExtensionDispatchTable deferredHostDispatch	= DeferredHostOperationsExtensionDispatchTable(renderContext);
	AccelerationStructureExtensionDispatchTable accelStructDispatch		= AccelerationStructureExtensionDispatchTable(renderContext);
	RayTracingExtensionDispatchTable rayTracingDispatch					= RayTracingExtensionDispatchTable(renderContext);
};

/// @brief The raytracing pipeline builder manages pipeline create info state for ray tracing pipelines.
class RayTracingPipelineBuilder
{
public:
	/// @brief Create a new raytracing pipeline builder.
	/// @param ctx Ray Tracing context to use.
	RayTracingPipelineBuilder(RayTracingContext& ctx);

	/// @brief Add a shader stage to this pipeline.
	/// @param stage Shader stage type.
	/// @param module Shader module handle.
	/// @param entryPoint Shader entrypoint name.
	/// @return A reference to this class.
	RayTracingPipelineBuilder& addShaderStage(
		VkShaderStageFlagBits stage,
		VkShaderModule module,
		const char* entryPoint = "main"
	);

	/// @brief Add a ray tracing shader group to this pipeline.
	/// @param type Shader Group type.
	/// @param generalShader General shader index.
	/// @param closestHitShader Closest hit shader index.
	/// @param anyHitShader Any hit shader index.
	/// @param intersectionShader Intersection shader index.
	/// @return A reference to this class.
	RayTracingPipelineBuilder& addRayTracingShaderGroup(
		VkRayTracingShaderGroupTypeKHR type,
		uint32_t generalShader = VK_SHADER_UNUSED_KHR,
		uint32_t closestHitShader = VK_SHADER_UNUSED_KHR,
		uint32_t anyHitShader = VK_SHADER_UNUSED_KHR,
		uint32_t intersectionShader = VK_SHADER_UNUSED_KHR
	);

	/// @brief Set the maximum recursion depth for raytracing (default is DEMO_MAX_RT_RECURSION_DEPTH)
	/// @param depth Maximum recursion depth.
	/// @return A reference to this class.
	RayTracingPipelineBuilder& setMaxRecursionDepth(uint32_t depth);

	/// @brief Add dynamic states to this pipeline.
	/// @param dynamicStates Dynamic states to add.
	/// @return A reference to this class.
	RayTracingPipelineBuilder& addDynamicStates(const std::vector<VkDynamicState>& dynamicStates);

	/// @brief Set the pipeline layout to use for creation.
	/// @param layout Pipeline Layout handle.
	/// @return A reference to this class.
	RayTracingPipelineBuilder& setLayout(VkPipelineLayout layout);

	/// @brief Set pipeline create flags to use.
	/// @param flags Flags to use.
	/// @return A reference to this class.
	RayTracingPipelineBuilder& setCreateFlags(VkPipelineCreateFlags flags);

	/// @brief Build a ray tracing pipeline using the configuration provided.
	/// @param cache Pipeline cache to use for building.
	/// @param deferredOperation Deferred Operation handle that allows postponing the actual build operation to a later point in time.
	/// @return A new vk pipeline handle.
	VkPipeline build(VkPipelineCache cache = VK_NULL_HANDLE, VkDeferredOperationKHR deferredOperation = VK_NULL_HANDLE);

private:
	RayTracingContext& m_ctx;
	VkPipelineCreateFlags m_flags = 0;
	std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages = {};
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_shaderGroups = {};
	uint32_t m_maxRecursionDepth = DEMO_DEFAULT_RT_RECURSION_DEPTH;
	std::vector<VkDynamicState> m_dynamicStates = {};
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

class AccelerationStructure
{
public:
	AccelerationStructure(RayTracingContext& ctx, VkAccelerationStructureTypeKHR type, size_t size);

	virtual ~AccelerationStructure();

	AccelerationStructure(const AccelerationStructure&) = delete;
	AccelerationStructure& operator=(const AccelerationStructure&) = delete;

	AccelerationStructure(AccelerationStructure&& other) noexcept;
	AccelerationStructure& operator=(AccelerationStructure&& other) noexcept;

private:
	void release();

public:
	hri::BufferResource buffer;
	VkAccelerationStructureKHR accelerationStructure	= VK_NULL_HANDLE;

private:
	RayTracingContext& m_ctx;
};

class AccelerationStructureGeometryBuilder
{
public:
	AccelerationStructureGeometryBuilder(RayTracingContext& ctx);

	AccelerationStructureGeometryBuilder& setBuildFlags(VkBuildAccelerationStructureFlagsKHR flags)
	{
		m_flags = flags;

		return *this;
	}

	AccelerationStructureGeometryBuilder& addGeometry(
		VkFormat vertexFormat,
		VkIndexType indexType,
		size_t vertexStride,
		uint32_t maxVertexIdx,
		const hri::BufferResource& vertexBuffer,
		const hri::BufferResource& indexBuffer,
		VkGeometryFlagsKHR flags = 0
	)
	{
		VkBufferDeviceAddressInfo vertexBufferAddressInfo = vertexBuffer.deviceAddressInfo();
		VkBufferDeviceAddressInfo indexBufferAddressInfo = indexBuffer.deviceAddressInfo();

		VkDeviceOrHostAddressConstKHR vertexData = {};
		vertexData.deviceAddress = vkGetBufferDeviceAddress(m_ctx.renderContext.device, &vertexBufferAddressInfo);

		VkDeviceOrHostAddressConstKHR indexData = {};
		indexData.deviceAddress = vkGetBufferDeviceAddress(m_ctx.renderContext.device, &indexBufferAddressInfo);

		VkAccelerationStructureGeometryTrianglesDataKHR triangles = VkAccelerationStructureGeometryTrianglesDataKHR{};
		triangles.vertexFormat = vertexFormat;
		triangles.vertexData = vertexData;
		triangles.vertexStride = vertexStride;
		triangles.maxVertex = maxVertexIdx;
		triangles.indexType = indexType;
		triangles.indexData = indexData;
		triangles.transformData = VkDeviceOrHostAddressConstKHR{};

		VkAccelerationStructureGeometryDataKHR geometryData = VkAccelerationStructureGeometryDataKHR{};
		geometryData.triangles = triangles;

		VkAccelerationStructureGeometryKHR geometry = VkAccelerationStructureGeometryKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		geometry.flags = flags;
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry = geometryData;

		m_geometries.push_back(geometry);
		return *this;
	}

	VkAccelerationStructureBuildGeometryInfoKHR getBuildGeometryInfo(VkBuildAccelerationStructureModeKHR buildMode) const
	{
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo = VkAccelerationStructureBuildGeometryInfoKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		buildInfo.flags = m_flags;
		buildInfo.mode = buildMode;
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
		buildInfo.geometryCount = static_cast<uint32_t>(m_geometries.size());
		buildInfo.pGeometries = m_geometries.data();
		buildInfo.scratchData = VkDeviceOrHostAddressKHR{};
		
		return buildInfo;
	}

private:
	RayTracingContext m_ctx;
	VkBuildAccelerationStructureFlagsKHR m_flags = 0;
	std::vector<VkAccelerationStructureGeometryKHR> m_geometries = {};
};
