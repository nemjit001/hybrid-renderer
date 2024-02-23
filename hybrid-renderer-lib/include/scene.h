#pragma once

#include <vector>
#include <map>

#include "camera.h"
#include "material.h"
#include "mesh.h"
#include "renderer_internal/render_context.h"

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

	/// @brief A scene node contains indices pointing to entries in the scene data arrays.
	struct SceneNode
	{
		uint32_t mesh		= HRI_INVALID_SCENE_INDEX;
		uint32_t material	= HRI_INVALID_SCENE_INDEX;
	};

	/// @brief A Renderable Object represents a mesh and material ready for rendering.
	struct RenderableObject
	{
		Float4x4 transform;
		Material* material;
		Mesh* mesh;
	};

	/// @brief The RenderableScene structure contains scene parameters as set in the Scene that produced it, as well as a list of renderable objects.
	struct RenderableScene
	{
		SceneParameters sceneParameters;
		std::vector<RenderableObject> renderables;
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
		Scene(SceneParameters sceneParameters, SceneData&& sceneData, const std::vector<SceneNode>& nodes);

		/// @brief Destroy this Scene object.
		virtual ~Scene() = default;

		// Allow move semantics
		Scene(Scene&&) = default;
		Scene& operator=(Scene&&) = default;

		/// @brief Generate a renderable scene based on the camera position in the world.
		/// @param camera Camera used for rendering the world.
		/// @return A renderable scene structure.
		RenderableScene generateRenderableScene(hri::Camera& camera);

	private:
		SceneParameters m_sceneParameters	= SceneParameters{};
		SceneData m_sceneData				= SceneData{};
		std::vector<SceneNode> m_nodes		= {};
	};
}
