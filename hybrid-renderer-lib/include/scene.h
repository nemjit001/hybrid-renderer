#pragma once

#include <vector>
#include <map>

#include "material.h"
#include "mesh.h"
#include "renderer_internal/render_context.h"
#include "renderer_internal/buffer.h"

#define HRI_INVALID_SCENE_INDEX	(uint32_t)(~0)

namespace hri
{
	/// @brief The SceneParameters structure contains parameters controlling
	///		global scene settings.
	struct SceneParameters
	{
		// TODO: Add actual scene settings
	};

	/// @brief The SceneData struct contains data to be referenced by scene nodes.
	struct SceneData
	{
		std::vector<Mesh> meshes			= {};
		std::vector<Material> materials		= {};
	};

	/// @brief A scene node contains references into the scene data arrays.
	struct SceneNode
	{
		uint32_t mesh		= HRI_INVALID_SCENE_INDEX;
		uint32_t material	= HRI_INVALID_SCENE_INDEX;
	};

	struct BatchedSceneData
	{
		std::unordered_map<uint32_t, std::vector<GPUMesh>> batchedMeshes = {};
	};

	/// @brief The Scene class represents the renderable world and its contents.
	class Scene
	{
	public:
		/// @brief Instantiate an empty scene with default parameters.
		Scene() = default;

		/// @brief Instantiate a new Scene object.
		/// @param sceneParameters SceneParameters to use.
		/// @param sceneData Data list containing resources that the scene nodes point to.
		/// @param nodes SceneNode list with scene object entries.
		Scene(SceneParameters sceneParameters, const SceneData& sceneData, const std::vector<SceneNode>& nodes);

		/// @brief Destroy this Scene object.
		virtual ~Scene() = default;

		BatchedSceneData generateBatchedSceneData(RenderContext* ctx);

		void destroyBatchedSceneData(RenderContext* ctx, BatchedSceneData& sceneData);

	private:
		SceneParameters m_sceneParameters	= SceneParameters{};
		SceneData m_sceneData				= SceneData{};
		std::vector<SceneNode> m_nodes		= {};
	};
}
