#include "scene.h"

#include <hybrid_renderer.h>
#include <string>

SceneGraph::SceneGraph(
	std::vector<Material>&& materials,
	std::vector<hri::Mesh>&& meshes,
	std::vector<SceneNode>&& nodes
)
	:
	nodes(std::move(nodes))
{
	//
}
