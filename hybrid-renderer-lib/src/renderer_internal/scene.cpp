#include "renderer_internal/scene.h"

#include <vector>

#include "renderer_internal/mesh.h"

using namespace hri;

Scene::Scene(SceneParameters sceneParameters, const std::vector<SceneNode>& nodes)
	:
	m_sceneParameters(sceneParameters),
	m_nodes(nodes)
{
	//
}
