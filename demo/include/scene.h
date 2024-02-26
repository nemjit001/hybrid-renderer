#pragma once

#include <hybrid_renderer.h>
#include <string>

#include "detail/raytracing.h"
#include "material.h"

#define INVALID_SCENE_ID	(size_t)(~0)
#define DEFAULT_LOD_FAR		100.0f
#define MAX_LOD_LEVELS		3

struct SceneParameters
{
	float nearPoint = 0.0f;
	float farPoint	= DEFAULT_LOD_FAR;
};

struct SceneTransform
{
	hri::Float3 position	= hri::Float3(0.0f, 0.0f, 0.0f);
	hri::Float3 rotation	= hri::Float3(0.0f, 0.0f, 0.0f);
	hri::Float3 scale		= hri::Float3(1.0f, 1.0f, 1.0f);

	inline hri::Float4x4 modelMatrix() const;
};

struct SceneNode
{
	typedef size_t SceneId;

	std::string name					= "No Name";
	SceneTransform transform			= SceneTransform{};
	SceneId material					= INVALID_SCENE_ID;
	SceneId meshLODs[MAX_LOD_LEVELS]	= { INVALID_SCENE_ID, INVALID_SCENE_ID, INVALID_SCENE_ID };
};

struct Renderable
{
	hri::Float4x4 modelMatrix;
	hri::Mesh* pMesh;
};

class SceneGraph
{
public:
	SceneGraph(
		raytracing::RayTracingContext& ctx,
		std::vector<Material>&& materials,
		std::vector<hri::Mesh>&& meshes,
		std::vector<SceneNode>&& nodes
	);

	virtual ~SceneGraph() = default;

	void update(float deltaTime);

	// FIXME: refactor for batching & acceleration structure building / updating
	std::vector<Renderable> generateDrawData(const hri::Camera& camera);

private:
	SceneNode::SceneId calculateLODLevel(const hri::Camera& camera, const SceneNode& node);

public:
	SceneParameters parameters;
	std::vector<Material> materials;
	std::vector<hri::Mesh> meshes;
	std::vector<SceneNode> nodes;

private:
	raytracing::RayTracingContext& m_ctx;
	raytracing::ASBuilder m_asBuilder;
};

class SceneLoader
{
public:
	static SceneGraph load(raytracing::RayTracingContext& context, const std::string& path);

private:
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
