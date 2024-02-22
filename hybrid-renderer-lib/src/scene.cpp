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

BatchedSceneData Scene::generateBatchedSceneData(RenderContext* ctx)
{
	assert(ctx != nullptr);

	std::unordered_map<uint32_t, std::vector<GPUMesh>> batchedMeshes = {};
	for (size_t materialIdx = 0; materialIdx < m_sceneData.materials.size(); materialIdx++)
	{
		for (auto const& node : m_nodes)
		{
			auto const& mesh = m_sceneData.meshes[node.mesh];

			if (node.material == materialIdx)
			{
				batchedMeshes[materialIdx].push_back(mesh.createGPUMesh(ctx));
			}
		}
	}

	return BatchedSceneData{
		batchedMeshes
	};
}

void Scene::destroyBatchedSceneData(RenderContext* ctx, BatchedSceneData& sceneData)
{
	for (auto& [ material, meshes ] : sceneData.batchedMeshes)
	{
		for (auto& mesh : meshes)
		{
			mesh.destroy();
		}
	}

	sceneData.batchedMeshes.clear();
}
