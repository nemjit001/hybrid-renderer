#include "scene.h"

#include <vector>

#include "mesh.h"

using namespace hri;

Scene::Scene(SceneParameters sceneParameters, const SceneData& sceneData, const std::vector<SceneNode>& nodes)
	:
	m_sceneParameters(sceneParameters),
	m_sceneData(sceneData),
	m_nodes(nodes)
{
	//
}
