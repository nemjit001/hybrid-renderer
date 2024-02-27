#pragma once

#include <hybrid_renderer.h>
#include <vector>

#include "demo.h"
#include "detail/dispatch.h"

namespace raytracing
{
	struct RayTracingContext;

	/// @brief Get the device address from a buffer resource.
	/// @param ctx Ray Tracing Context to use.
	/// @param resource Buffer Resource to query.
	/// @return A Device Address.
	VkDeviceAddress getDeviceAddress(const RayTracingContext& ctx, const hri::BufferResource& resource);

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
		DeferredHostOperationsExtensionDispatchTable deferredHostDispatch = DeferredHostOperationsExtensionDispatchTable(renderContext);
		AccelerationStructureExtensionDispatchTable accelStructDispatch = AccelerationStructureExtensionDispatchTable(renderContext);
		RayTracingExtensionDispatchTable rayTracingDispatch = RayTracingExtensionDispatchTable(renderContext);
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

		/// @brief Set the maximum recursion depth for raytracing (default is DEMO_DEFAULT_RT_RECURSION_DEPTH)
		/// @param depth Maximum recursion depth.
		/// @return A reference to this class.
		RayTracingPipelineBuilder& setMaxRecursionDepth(uint32_t depth = DEMO_DEFAULT_RT_RECURSION_DEPTH);

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

	/// @brief The Shader Binding Table is used by a raytracing pipeline to reference its bounds shaders.
	class ShaderBindingTable
	{
	public:
		/// @brief Shader Group Handle Info contains sizes needed to calculate shader region offsets.
		struct ShaderGroupHandleInfo
		{
			size_t handleSize			= 0;
			size_t baseAlignment		= 0;
			size_t alignedHandleSize	= 0;
		};

	public:
		/// @brief Create a new Shader Binding Table.
		/// @param ctx Ray Tracing Context to use.
		/// @param rayGenRegion Ray Gen region, 1 shader, MUST be set.
		/// @param rayMissRegion Ray Miss region, optional shaders.
		/// @param rayHitRegion Ray Hit region, optional shaders.
		/// @param rayCallRegion Callable shader region, optional shaders.
		ShaderBindingTable(
			RayTracingContext& ctx,
			VkStridedDeviceAddressRegionKHR rayGenRegion,
			VkStridedDeviceAddressRegionKHR rayMissRegion = VkStridedDeviceAddressRegionKHR{},
			VkStridedDeviceAddressRegionKHR rayHitRegion = VkStridedDeviceAddressRegionKHR{},
			VkStridedDeviceAddressRegionKHR rayCallRegion = VkStridedDeviceAddressRegionKHR{}
		);

		/// @brief Destroy this SBT.
		virtual ~ShaderBindingTable() = default;

		/// @brief Populate this SBT.
		/// @param shaderGroupHandles Shader Group Handles fetched from pipeline, shaders groups MUST be in order Ray Gen,
		///		Ray Miss, Ray Hit, Ray Call.
		/// @param missHandleCount Number of miss shaders.
		/// @param hitHandleCount Number of hit shaders.
		/// @param callHandleCount Number of callable shaders.
		void populateSBT(
			const std::vector<uint8_t>& shaderGroupHandles,
			size_t missHandleCount = 0,
			size_t hitHandleCount = 0,
			size_t callHandleCount = 0
		);

		/// @brief Get shader group handle info from a ray tracing context.
		/// @param ctx Context to use.
		/// @return This context's Shader Group Handle Info.
		static ShaderGroupHandleInfo getShaderGroupHandleInfo(RayTracingContext& ctx);

	private:
		/// @brief Get the internal address region offset.
		/// @param regionIdx Region Index to use.
		/// @return The address offset.
		size_t getAddressRegionOffset(size_t regionIdx) const;

		/// @brief Get a shader group handle offset from a handle array by index.
		/// @param pHandles Shader group handle list.
		/// @param index Index to retrieve.
		/// @return A shader group handle pointer offset.
		void* getShaderGroupHandleOffset(const void* pHandles, size_t index) const;

	public:
		static const size_t rayGenRegionIdx			= 0;
		static const size_t rayMissRegionIdx		= 1;
		static const size_t rayHitRegionIdx			= 2;
		static const size_t rayCallRegionIdx		= 3;
		VkStridedDeviceAddressRegionKHR regions[4]	= {};

	private:
		RayTracingContext& m_ctx;
		ShaderGroupHandleInfo m_handleInfo;
		size_t m_SBTSize;
		hri::BufferResource m_SBTBuffer;
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
		VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

	private:
		RayTracingContext& m_ctx;
	};

	/// @brief The Acceleration Structure Builder handles all things to do with acceleration structure building.
	class ASBuilder
	{
	public:
		/// @brief The AS Size Info contains max build sizes for Acceleration Structures.
		struct ASSizeInfo
		{
			size_t maxBuildScratchBufferSize		= 0;
			size_t maxUpdateScratchBufferSize		= 0;
			size_t totalAccelerationStructureSize	= 0;
		};

		/// @brief AS Build Info contains geometry info, build ranges, and build sizes for structures.
		struct ASBuildInfo
		{
			VkAccelerationStructureBuildGeometryInfoKHR geometryInfo	= {};
			VkAccelerationStructureBuildRangeInfoKHR const* buildRanges = nullptr;
			VkAccelerationStructureBuildSizesInfoKHR buildSizes			= {};
		};

		/// @brief The AS Input contains input for generating build info & build sizes.
		struct ASInput
		{
			VkBuildAccelerationStructureFlagsKHR buildFlags = 0;
			std::vector <VkAccelerationStructureGeometryKHR> geometry			= {};
			std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildOffsets	= {};
		};

	public:
		/// @brief Create a new AS Builder.
		/// @param ctx Ray Tracing Context to use.
		ASBuilder(RayTracingContext& ctx);

		/// @brief Destroy this AS Builder.
		virtual ~ASBuilder() = default;

		/// @brief Convert a Mesh object's data to AS Input.
		/// @param mesh Mesh to use for AS Input.
		/// @param buildFlags Build Flags to use.
		/// @param geometryFlags Geometry Flags to use.
		/// @return Generated AS Input.
		ASInput objectToGeometry(
			const hri::Mesh& mesh,
			VkBuildAccelerationStructureFlagsKHR buildFlags = 0,
			VkGeometryFlagsKHR geometryFlags = 0
		) const;

		/// @brief Convert an instance buffer to AS Input.
		/// @param instanceBuffer Instance buffer of VkAccelerationStructureInstanceKHRs.
		/// @param instanceCount Number of instances stored in the instance buffer.
		/// @param arrayOfPointers Set to true if the instance buffer is an array of pointers.
		/// @param buildFlags Build Flags to use.
		/// @param geometryFlags Geometry Flags to use.
		/// @return Generated AS Input.
		ASInput instancesToGeometry(
			const hri::BufferResource& instanceBuffer,
			size_t instanceCount,
			bool arrayOfPointers,
			VkBuildAccelerationStructureFlagsKHR buildFlags = 0,
			VkGeometryFlagsKHR geometryFlags = 0
		) const;

		
		ASBuilder::ASBuildInfo generateASBuildInfo(
			const ASInput& input,
			ASSizeInfo& sizeInfo,
			VkAccelerationStructureTypeKHR asType,
			VkBuildAccelerationStructureModeKHR buildMode,
			VkBuildAccelerationStructureFlagsKHR flags
		) const;

		/// @brief Generate build infos from inputs.
		/// @param inputs Inputs to generate build infos for.
		/// @param sizeInfo Size info to populate during generation.
		/// @param asType Acceleration Structure type to generate build infos for.
		/// @param buildMode Build mode (Build or Update).
		/// @param flags Global build flags to use along with input build flags.
		/// @return A list of build infos. MUST live shorter than the AS Inputs used to generate it.
		std::vector<ASBuildInfo> generateASBuildInfo(
			const std::vector<ASInput>& inputs,
			ASSizeInfo& sizeInfo,
			VkAccelerationStructureTypeKHR asType,
			VkBuildAccelerationStructureModeKHR buildMode,
			VkBuildAccelerationStructureFlagsKHR flags = 0
		) const;

		/// @brief Build an acceleration structure.
		/// @param commandBuffer Command buffer to use.
		/// @param buildInfo Build Info to use.
		/// @param structure Structure handle to build for.
		/// @param scratchDataAddress Scratch data adress to use.
		void cmdBuildAccelerationStructure(
			VkCommandBuffer commandBuffer,
			const ASBuildInfo& buildInfo,
			VkAccelerationStructureKHR structure,
			VkDeviceAddress scratchDataAddress
		) const;

		/// @brief Build a list of acceleration structures.
		/// @param commandBuffer Command buffer to use.
		/// @param buildInfos Build info list to use.
		/// @param structures Acceleration structure handles to use for build.
		/// @param scratchDataAddress Shared scratch data buffer to use for build.
		void cmdBuildAccelerationStructures(
			VkCommandBuffer commandBuffer,
			const std::vector<ASBuildInfo>& buildInfos,
			std::vector<VkAccelerationStructureKHR>& structures,
			VkDeviceAddress scratchDataAddress
		) const;

		// TODO acceleration structure compaction

	private:
		RayTracingContext& m_ctx;
	};
}
