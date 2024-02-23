#pragma once

#include <hybrid_renderer.h>
#include <string>

#include "material.h"

#define INVALID_SCENE_ID	(size_t)(~0)
#define MAX_LOD_LEVELS		3

struct SceneTransform
{
	hri::Float3 position	= hri::Float3(0.0f, 0.0f, 0.0f);
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

class SceneGraph
{
public:
	SceneGraph(
		std::vector<Material>&& materials,
		std::vector<hri::Mesh>&& meshes,
		std::vector<SceneNode>&& nodes
	);

	virtual ~SceneGraph() = default;

public:
	std::vector<SceneNode> nodes;
};

inline hri::Float4x4 SceneTransform::modelMatrix() const
{
	hri::Float4x4 out = hri::Float4x4(1.0f);
	out = glm::translate(out, static_cast<vec3>(position));
	out = glm::scale(out, static_cast<vec3>(scale));
	// TODO: rotation

	return out;
}
