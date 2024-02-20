#pragma once

#include <vector>

#include "material.h"
#include "mesh.h"

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
		std::vector<Mesh> m_meshes			= {};
		std::vector<Material> m_materials	= {};
	};

	/// @brief A scene node contains references into the scene data arrays.
	struct SceneNode
	{
		uint32_t mesh		= HRI_INVALID_SCENE_INDEX;
		uint32_t material	= HRI_INVALID_SCENE_INDEX;
	};

	/// @brief The Scene class represents the renderable world and its contents.
	class Scene
	{
	public:
		/// @brief Instantiate an empty scene with default parameters.
		Scene() = default;

		/// @brief Instantiate a new Scene object.
		/// @param sceneParameters SceneParameters to use.
		/// @param meshes Mesh list with all meshes in the scene.
		/// @param materials Material list with all materials in the scene.
		/// @param nodes SceneNode list with scene object entries.
		Scene(SceneParameters sceneParameters, const SceneData& sceneData, const std::vector<SceneNode>& nodes);

		/// @brief Destroy this Scene object.
		virtual ~Scene() = default;

	private:
		SceneParameters m_sceneParameters	= SceneParameters{};
		SceneData m_sceneData				= SceneData{};
		std::vector<SceneNode> m_nodes		= {};
	};
}