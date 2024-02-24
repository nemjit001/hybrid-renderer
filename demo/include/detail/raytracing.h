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

	RayTracingContext(RayTracingContext&& other) noexcept;
	RayTracingContext& operator=(RayTracingContext&& other) noexcept;

	hri::RenderContext& renderContext;
	DeferredHostOperationsExtensionDispatchTable deferredHostDispatch	= DeferredHostOperationsExtensionDispatchTable(renderContext);
	AccelerationStructureExtensionDispatchTable accelStructDispatch		= AccelerationStructureExtensionDispatchTable(renderContext);
	RayTracingExtensionDispatchTable rayTracingDispatch					= RayTracingExtensionDispatchTable(renderContext);
};

struct RayTracingUtils
{
	static VkDeviceOrHostAddressKHR getDeviceAddress(const RayTracingContext& ctx, const hri::BufferResource& resource);

	static VkDeviceOrHostAddressConstKHR getConstDeviceAddress(const RayTracingContext& ctx, const hri::BufferResource& resource);
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

/// @brief An acceleration structure is used for ray tracing to improve ray traversal performance.
class AccelerationStructure
{
public:
	/// @brief Create a new acceleration structure.
	/// @param ctx Ray Tracing Context to use.
	/// @param type Acceleration structure type.
	/// @param size Size of the acceleration structure.
	AccelerationStructure(RayTracingContext& ctx, VkAccelerationStructureTypeKHR type, size_t size);

	/// @brief Destroy this acceleration structure.
	virtual ~AccelerationStructure();

	// Disallow copy behaviour
	AccelerationStructure(const AccelerationStructure&) = delete;
	AccelerationStructure& operator=(const AccelerationStructure&) = delete;

	// Allow move semantics
	AccelerationStructure(AccelerationStructure&& other) noexcept;
	AccelerationStructure& operator=(AccelerationStructure&& other) noexcept;

private:
	/// @brief Release resources held by this acceleration structure.
	void release();

public:
	hri::BufferResource buffer;
	VkAccelerationStructureKHR accelerationStructure	= VK_NULL_HANDLE;

private:
	RayTracingContext& m_ctx;
};

/// @brief The Acceleration Structure Geometry Builder handles state for generating as geometry,
class AccelerationStructureGeometryBuilder
{
public:
	/// @brief Create a new Acceleration Structure Geometry Builder.
	/// @param ctx Ray Tracing Context to use.
	AccelerationStructureGeometryBuilder(RayTracingContext& ctx);

	/// @brief Set build flags for this acceleration structure geometry.
	/// @param flags Build flags to set.
	/// @return A reference to this class.
	AccelerationStructureGeometryBuilder& setBuildFlags(VkBuildAccelerationStructureFlagsKHR flags);

	/// @brief Add AABB geometry data to this acceleration structure geometry.
	/// @param aabbStride Stride of the AABB.
	/// @param aabbBuffer Buffer resource containing AABB data.
	/// @param flags Geometry flags to set.
	/// @return A reference to this class.
	AccelerationStructureGeometryBuilder& addGeometry(
		size_t aabbStride,
		const hri::BufferResource& aabbBuffer,
		VkGeometryFlagsKHR flags = 0
	);

	/// @brief Add Triangle geometry data to this acceleration structure geometry.
	/// @param vertexFormat Format of the vertex position data.
	/// @param indexType Type of index used in the indices buffer.
	/// @param vertexStride Size of the vertex data type.
	/// @param maxVertexIdx Maximum vertex index in the index buffer.
	/// @param vertexBuffer Vertex buffer resource.
	/// @param indexBuffer Index buffer resource.
	/// @param flags Geometry flags to set.
	/// @return A reference to this class.
	AccelerationStructureGeometryBuilder& addGeometry(
		VkFormat vertexFormat,
		VkIndexType indexType,
		size_t vertexStride,
		uint32_t maxVertexIdx,
		const hri::BufferResource& vertexBuffer,
		const hri::BufferResource& indexBuffer,
		VkGeometryFlagsKHR flags = 0
	);

	/// @brief Get the Build Geometry Info generated by this builder.
	/// @param buildMode Build mode used for the acceleration structure.
	/// @param accelerationStructure Optional acceleration structure handle.
	///		Required for building, not required for size queries.
	/// @return An Acceleration Structure Build Geometry Info structure based on the provided configuration data.
	VkAccelerationStructureBuildGeometryInfoKHR getBuildGeometryInfo(
		VkBuildAccelerationStructureModeKHR buildMode,
		VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE
	) const;

private:
	RayTracingContext& m_ctx;
	VkBuildAccelerationStructureFlagsKHR m_flags = 0;
	std::vector<VkAccelerationStructureGeometryKHR> m_geometries = {};
};
