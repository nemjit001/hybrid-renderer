#pragma once

#include <hybrid_renderer.h>
#include <array>
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

	/// @brief Convert a 4x4 matrix to a 3x4 matrix.
	/// @param mat Matrix to convert.
	/// @return A vulkan 3x4 transform matrix.
	VkTransformMatrixKHR toTransformMatrix(const hri::Float4x4& mat);

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

		inline const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages() const { return m_shaderStages; }

		inline const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& shaderGroups() const { return m_shaderGroups; }

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
		/// @brief SBT regions contain shader module handles that are called during ray tracing.
		enum SBTShaderGroup
		{
			SGRayGen,
			SGMiss,
			SGHit,
			SGCall
		};

		/// @brief Shader Group Handle Info contains sizes needed to calculate shader region offsets.
		struct ShaderGroupHandleInfo
		{
			uint32_t handleSize			= 0;
			uint32_t baseAlignment		= 0;
			uint32_t alignedHandleSize	= 0;
		};

	public:
		/// @brief Create a new SBT.
		/// @param ctx Ray Tracing Context to use.
		/// @param pipeline Ray Tracing Pipeline for which to generate an SBT.
		/// @param pipelineBuilder Pipeline Builder used to create the RT Pipeline.
		ShaderBindingTable(
			RayTracingContext& ctx,
			VkPipeline pipeline,
			const RayTracingPipelineBuilder& pipelineBuilder
		);

		/// @brief Get Shader Group Handle info such as size, stride, and aligned size.
		/// @param ctx Ray Tracing Context to use.
		/// @return A ShaderGroupHandleInfo struct.
		static ShaderGroupHandleInfo getShaderGroupHandleInfo(RayTracingContext& ctx);

		/// @brief Get an SBT region.
		/// @param group SBT Shader Group for which to fetch the region.
		/// @param offset Offset into the SBT group start.
		/// @return A strided device address region.
		const VkStridedDeviceAddressRegionKHR getRegion(SBTShaderGroup group, uint32_t offset = 0) const;

		/// @brief Get the size of an SBT Shader Group.
		/// @param group Group to fetch the size for.
		/// @return The group size.
		inline uint32_t size(SBTShaderGroup group) const {
			return group == SBTShaderGroup::SGRayGen ?
				m_shaderGroupStrides[group] : m_shaderGroupStrides[group] * indexCount(group);
		}
		
		/// @brief Get the stride of an SBT Shader Group.
		/// @param group Group to fetch the stride for.
		/// @return The group stride.
		inline uint32_t stride(SBTShaderGroup group) const { return m_shaderGroupStrides[group]; }

		/// @brief Get the number of shader stage indices in a shader group.
		/// @param group Group to fetch the number of indices for.
		/// @return The index count of a group.
		inline uint32_t indexCount(SBTShaderGroup group) const { return static_cast<uint32_t>(m_shaderGroupIndices[group].size()); }

	private:
		/// @brief Tet a device address for a shader group.
		/// @param group 
		/// @return A device address or 0 if no buffer was allocated for this group.
		VkDeviceAddress getGroupDeviceAddress(SBTShaderGroup group) const;

		/// @brief Get a shader handle's byte offset from a handle array.
		uint8_t* getShaderHandleOffset(uint8_t* pHandles, size_t idx) const;

		/// @brief Fill out the shader group indices array using the pipeline builder provided.
		/// @param pipelineBuilder The pipeline builder containing shader groups.
		void getShaderGroupIndices(const RayTracingPipelineBuilder& pipelineBuilder);

		/// @brief Fill out the shader group strides array using the group indices.
		void getShaderGroupStrides();

	private:
		RayTracingContext& m_ctx;
		ShaderGroupHandleInfo m_handleInfo = ShaderGroupHandleInfo{};
		std::array<std::vector<uint32_t>, 4> m_shaderGroupIndices;
		std::array<uint32_t, 4> m_shaderGroupStrides;
		std::array<uint32_t, 4> m_shaderGroupSizes;
		std::array<std::vector<uint8_t>, 4> m_sbtData;
		
		std::unordered_map<SBTShaderGroup, hri::BufferResource> m_buffers;
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

		/// @brief Generate build info from input.
		/// @param input Input to generate build info for.
		/// @param sizeInfo Size info to populate during generation, will not be cleared in this function call.
		/// @param asType Acceleration Structure type to generate build infos for.
		/// @param buildMode Build mode (Build or Update).
		/// @param flags Global build flags to use along with input build flags.
		/// @return A build info structure. MUST live shorter than the AS Inputs used to generate it.
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
