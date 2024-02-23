#include "scene.h"

#include <vector>
#include <map>

#include "camera.h"
#include "material.h"
#include "mesh.h"
#include "renderer_internal/render_context.h"

using namespace hri;

Scene::Scene(RenderContext* ctx, SceneParameters sceneParameters, const SceneData& sceneData, const std::vector<SceneNode>& nodes)
	:
	m_sceneParameters(sceneParameters),
	m_sceneData(sceneData),
	m_nodes(nodes)
{
	assert(ctx != nullptr);
	m_gpuMeshes.reserve(m_sceneData.meshes.size());

	for (auto const& mesh : m_sceneData.meshes)
	{
		m_gpuMeshes.push_back(mesh.createGPUMesh(ctx));
	}
}

Scene::~Scene()
{
	for (auto& mesh : m_gpuMeshes)
	{
		mesh.destroy();
	}
}

RenderableScene Scene::generateRenderableScene(hri::Camera& camera) const
{
	std::vector<RenderableObject> renderables = {};

	for (size_t matIdx = 0; matIdx < m_sceneData.materials.size(); matIdx++)
	{
		for (auto const& node : m_nodes)
		{
			auto const& material = m_sceneData.materials[node.mesh];
			auto const& mesh = m_gpuMeshes[node.mesh];

			if (node.material == matIdx)
			{
				// TODO: check dist camera -> mesh for LOD selection

				RenderableObject renderObject = RenderableObject{
					Float4x4(1.0f),
					material,
					mesh,
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
