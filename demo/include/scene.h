#pragma once

#include <hybrid_renderer.h>
#include <string>

#include "detail/raytracing.h"
#include "material.h"

#define INVALID_SCENE_ID	(size_t)(~0)
#define MAX_LOD_LEVELS		3
#define INSTANCE_MASK_BITS	8
#define VALID_MASK			((1 << INSTANCE_MASK_BITS) - 1)

/// @brief Scene parameters allow modifying LOD selection.
struct SceneParameters
{
	float lodBias				= 0.0f;
	float transitionInterval	= 0.5f;
	float nearPoint				= 0.0f;
	float farPoint				= 100.0f;
};

/// @brief A Scene Transform is a simple collection of vectors representing translation, rotation, and scale.
struct SceneTransform
{
	hri::Float3 position	= hri::Float3(0.0f, 0.0f, 0.0f);
	hri::Float3 rotation	= hri::Float3(0.0f, 0.0f, 0.0f);
	hri::Float3 scale		= hri::Float3(1.0f, 1.0f, 1.0f);

	/// @brief Create a 4x4 transformation matrix representing the model transfom.
	/// @return An hri::Float4x4 matrix.
	inline hri::Float4x4 modelMatrix() const;
};

/// @brief A Scene Node represents an entry into the SceneGraph, with data relevant for rendering.
struct SceneNode
{
	typedef size_t SceneId;

	std::string name					= "No Name";
	SceneTransform transform			= SceneTransform{};
	SceneId material					= INVALID_SCENE_ID;
	uint32_t numLods					= 0;
	SceneId meshLODs[MAX_LOD_LEVELS]	= { INVALID_SCENE_ID, INVALID_SCENE_ID, INVALID_SCENE_ID };
};

/// @brief A render instance a model matrix and a pointer to instance data.
struct RenderInstance
{
	hri::Float4x4 modelMatrix;
	float lodBlendFactor;
	uint32_t instanceIdLOD0;
	uint32_t instanceIdLOD1;
};

/// @brief A LightArrayEntry is an index pointing to a light's mesh, used for importance sampling scene lights.
struct LightArrayEntry
{
	uint32_t lightIdx;
};

/// @brief Render Instance Data contains offsets into scene buffers & buffer addresses for objects.
struct RenderInstanceData
{
	uint32_t materialIdx;
	uint32_t indexCount;
	VkDeviceAddress vertexBufferAddress;
	VkDeviceAddress indexBufferAddress;
};

/// @brief Scene buffers store device local resources for a scene
struct SceneBuffers
{
	hri::BufferResource instanceDataSSBO;
	hri::BufferResource materialSSBO;
};

/// @brief The Scene Acceleration Structure Manager abstracts away some tedious setup for acceleration structure building.
class SceneASManager
{
public:
	/// @brief Create a new AS Manager.
	/// @param ctx Ray Tracing Context to use.
	SceneASManager(raytracing::RayTracingContext& ctx);

	/// @brief Destroy this AS Manager.
	virtual ~SceneASManager() = default;

	/// @brief Check if the TLAS should be reallocated.
	/// @param tlas TLAS to check for.
	/// @param instances Instances that should fit in the TLAS.
	/// @param blasList BLAS list with data for instances.
	/// @return A boolean indicating realloc.
	bool shouldReallocTLAS(
		const raytracing::AccelerationStructure& tlas,
		const std::vector<RenderInstance>& instances,
		const std::vector<raytracing::AccelerationStructure>& blasList
	) const;

	/// @brief Create a TLAS using a BLAS and instance list.
	/// @param instances Instances to use.
	/// @param blasList BLASses to use.
	/// @return A newly created TLAS.
	raytracing::AccelerationStructure createTLAS(
		const std::vector<RenderInstance>& instances,
		const std::vector<raytracing::AccelerationStructure>& blasList
	);

	/// @brief Generate a BLAS list from a list of meshes. Should be done once for all meshes in scene.
	/// @param meshes Meshes to generate BLASses for.
	/// @return A vector of BLAS structues.
	std::vector<raytracing::AccelerationStructure> createBLASList(const std::vector<hri::Mesh>& meshes);

	/// @brief Record TLAS building commands.
	/// @param commandBuffer Command buffer to record into.
	/// @param instances Instances to use for building.
	/// @param blasList BLAS list with per instance data.
	/// @param tlas TLAS to build.
	void cmdBuildTLAS(
		VkCommandBuffer commandBuffer,
		const std::vector<RenderInstance>& instances,
		const std::vector<raytracing::AccelerationStructure>& blasList,
		raytracing::AccelerationStructure& tlas
	) const;

	/// @brief Record BLAS building commands.
	/// @param commandBuffer Command buffer to record into.
	/// @param instances Instances to build BLASses for.
	/// @param meshes Meshes associated with the BLASses.
	/// @param blasList BLAS List to build.
	void cmdBuildBLASses(
		VkCommandBuffer commandBuffer,
		const std::vector<RenderInstance>& instances,
		const std::vector<hri::Mesh>& meshes,
		const std::vector<raytracing::AccelerationStructure>& blasList
	) const;

private:
	/// @brief Generate an instance buffer for a TLAS.
	/// @param instances Instances to store in TLAS.
	/// @param blasList BLAS list with instance data.
	/// @return A buffer resource containing instance data.
	hri::BufferResource generateTLASInstances(
		const std::vector<RenderInstance>& instances,
		const std::vector<raytracing::AccelerationStructure>& blasList
	) const;

	/// @brief Generate a list of ASInputs from a list of meshes.
	/// @param meshes Meshes to generate inputs from.
	/// @return A list of AS Inputs for BLAS building.
	std::vector<raytracing::ASBuilder::ASInput> generateBLASInputs(const std::vector<hri::Mesh>& meshes) const;

	uint32_t generateLODMask(const RenderInstance& instance) const;

private:
	raytracing::RayTracingContext& m_ctx;
	raytracing::ASBuilder m_asBuilder;
};

class SceneGraph
{
public:
	/// @brief Create a new scene graph.
	/// @param ctx Ray Tracing Context to use.
	/// @param materials Materials in the scene.
	/// @param meshes Meshes in the scene.
	/// @param nodes SceneNodes list representing mesh & material instance locations.
	SceneGraph(
		raytracing::RayTracingContext& ctx,
		std::vector<Material>&& materials,
		std::vector<hri::Mesh>&& meshes,
		std::vector<SceneNode>&& nodes
	);

	/// @brief Destroy this scene graph.
	virtual ~SceneGraph() = default;

	/// @brief Update the SceneGraph.
	/// @param deltaTime Time delta for this frame.
	void update(float deltaTime);

	/// @brief Retrieve cached instance list generated by the latest call to SceneGraph::generateRenderInstanceList.
	/// @return Cached instance data.
	const std::vector<RenderInstance>& getRenderInstanceList() const;

	/// @brief Generate instance list based on camera position, calculating LOD levels & returning an instance list.
	/// @param camera Camera to use for LOD generation.
	/// @return A newly generated list of render instances.
	const std::vector<RenderInstance>& generateRenderInstanceList(const hri::Camera& camera);

	inline const RenderInstanceData& getInstanceData(size_t idx) const { return m_instanceData.at(idx); }

private:
	/// @brief Calculate the LOD level for a given scene node.
	/// @param camera Camera to use for LOD generation.
	/// @param node Node to calculate LOD for.
	/// @param LODBlendFactor blend factor between LODs
	/// @param LOD0 current lod level
	/// @param LOD1 next lod level
	void calculateLODLevel(const hri::Camera& camera, const SceneNode& node, float& LODBlendFactor, SceneNode::SceneId& LOD0, SceneNode::SceneId& LOD1);

public:
	SceneParameters parameters;
	std::vector<Material> materials;
	std::vector<hri::Mesh> meshes;
	std::vector<SceneNode> nodes;
	uint32_t lightCount;
	SceneBuffers buffers;

private:
	raytracing::RayTracingContext& m_ctx;
	raytracing::ASBuilder m_asBuilder;
	std::vector<RenderInstanceData> m_instanceData	= {};
	std::vector<RenderInstance> m_instances			= {};
};

/// @brief The SceneLoader handles loading scene files from disk.
class SceneLoader
{
public:
	/// @brief Load a new Scene File.
	/// @param context Ray Tracing Context to use.
	/// @param path Path to the scene file.
	/// @return A new SceneGraph generated from the scene file.
	static SceneGraph load(raytracing::RayTracingContext& context, const std::string& path);

private:
	/// @brief Load an OBJ Mesh from an OBJ file.
	/// @param path File path.
	/// @param name Name of the mesh to load.
	/// @param material Material reference optionally populated by the loader.
	/// @param vertices Vertex list reference populated by the loader.
	/// @param indices Index list reference populated by the loader.
	/// @param loadMaterial Boolean indicating whether the mesh's material should be loaded.
	/// @return A boolean indicating success or failure.
	static bool loadOBJMesh(
		const std::string& path,
		const std::string& name,
		Material& material,
		std::vector<hri::Vertex>& vertices,
		std::vector<uint32_t>& indices,
		bool loadMaterial
	);
};

inline hri::Float4x4 SceneTransform::modelMatrix() const
{
	hri::Float4x4 out = hri::Float4x4(1.0f);
	out = glm::translate(out, static_cast<vec3>(position));
	out = glm::rotate(
		glm::rotate(
			glm::rotate(
				out, hri::radians(rotation.y), static_cast<vec3>(HRI_WORLD_UP)
			), hri::radians(rotation.z), static_cast<vec3>(HRI_WORLD_FORWARD)
		), hri::radians(rotation.x), static_cast<vec3>(HRI_WORLD_RIGHT)
	);
	out = glm::scale(out, static_cast<vec3>(scale));

	return out;
}
