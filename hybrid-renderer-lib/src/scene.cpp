#include "scene.h"

#include <vector>
#include <map>

#include "camera.h"
#include "material.h"
#include "mesh.h"
#include "renderer_internal/render_context.h"

using namespace hri;

Scene::Scene(RenderContext* ctx, SceneParameters sceneParameters, SceneData&& sceneData, const std::vector<SceneNode>& nodes)
	:
	m_sceneParameters(sceneParameters),
	m_sceneData(std::move(sceneData)),
	m_nodes(nodes)
{
	//
}

RenderableScene Scene::generateRenderableScene(hri::Camera& camera)
{
	std::vector<RenderableObject> renderables = {};

	for (size_t matIdx = 0; matIdx < m_sceneData.materials.size(); matIdx++)
	{
		for (auto&& node : m_nodes)
		{
			auto& material = m_sceneData.materials[node.mesh];
			auto& mesh = m_sceneData.meshes[node.mesh];

			if (node.material == matIdx)
			{
				// TODO: check dist camera -> mesh for LOD selection

				RenderableObject renderObject = RenderableObject{
					Float4x4(1.0f),
					&material,
					&mesh,
				};

				renderables.push_back(renderObject);
			}
		}
	}

	return RenderableScene{
		m_sceneParameters,
		renderables,
	};
}
