#include "renderer_internal/scene.h"

#include <vector>

#include "renderer_internal/mesh.h"

using namespace hri;

Scene::Scene(SceneParameters sceneParameters, const std::vector<Mesh>& meshes, const std::vector<Material>& materials, const std::vector<SceneNode>& nodes)
	:
	m_sceneParameters(sceneParameters),
	m_meshes(meshes),
	m_materials(materials),
	m_nodes(nodes)
{
	//
}
