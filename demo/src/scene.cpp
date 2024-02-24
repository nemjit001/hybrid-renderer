#include "scene.h"

#include <hybrid_renderer.h>
#include <string>

SceneGraph::SceneGraph(
	std::vector<Material>&& materials,
	std::vector<hri::Mesh>&& meshes,
	std::vector<SceneNode>&& nodes
)
	:
	materials(std::move(materials)),
	meshes(std::move(meshes)),
	nodes(std::move(nodes))
{
	//
}

std::vector<Renderable> SceneGraph::generateDrawData(const hri::Camera& camera)
{
	// TODO: batch nodes by material, then by mesh
	std::vector<Renderable> renderables = {};

	for (auto const& node : nodes)
	{
		SceneNode::SceneId meshLOD = calculateLODLevel(camera, node);

		// TODO: upload mesh data into scene buffers, generate TLAS
		renderables.push_back(Renderable{
			node.transform.modelMatrix(),
			&meshes.at(meshLOD),
		});
	}

	return renderables;
}

SceneNode::SceneId SceneGraph::calculateLODLevel(const hri::Camera& camera, const SceneNode& node)
{
	hri::Float3 camToNode = node.transform.position - camera.position;
	const float nodeDist = hri::magnitude(camToNode);
	const float maxViewDist = camera.parameters.zFar - camera.parameters.zNear;
	const float segmentSize = maxViewDist / static_cast<float>(MAX_LOD_LEVELS);

	SceneNode::SceneId LODIndex = 0;
	for (size_t segmentIdx = 0; segmentIdx < MAX_LOD_LEVELS; segmentIdx++)
	{
		float maxSegmentDist = (segmentIdx + 1) * segmentSize;

		if (nodeDist < maxSegmentDist && node.meshLODs[segmentIdx] != INVALID_SCENE_ID)
			LODIndex = segmentIdx;
	}

	return node.meshLODs[LODIndex];
}
