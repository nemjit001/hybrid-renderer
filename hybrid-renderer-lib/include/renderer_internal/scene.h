#pragma once

#include <vector>

#include "renderer_internal/material.h"
#include "renderer_internal/mesh.h"

#define HRI_INVALID_SCENE_INDEX	(uint32_t)(~0)

namespace hri
{
	/// @brief The SceneParameters structure contains parameters controlling
	///		global scene settings.
	struct SceneParameters
	{
		// TODO: Add actual scene settings
	};

	/// @brief A scene node contains references into the scene mesh & material arrays.
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
		Scene(SceneParameters sceneParameters, const std::vector<Mesh>& meshes, const std::vector<Material>& materials, const std::vector<SceneNode>& nodes);

		/// @brief Destroy this Scene object.
		virtual ~Scene() = default;

	private:
		SceneParameters m_sceneParameters	= SceneParameters{};
		std::vector<Mesh> m_meshes			= {};
		std::vector<Material> m_materials	= {};
		std::vector<SceneNode> m_nodes		= {};
	};
}
