#pragma once

#include <vector>

#include "renderer_internal/mesh.h"

namespace hri
{
	/// @brief The SceneParameters structure contains parameters controlling
	///		global scene settings.
	struct SceneParameters
	{
		// TODO: Add actual scene settings
	};

	struct SceneNode
	{
		// TODO: Add mesh / transform / etc. to scene node
		std::vector<SceneNode> children = {};
	};

	/// @brief The Scene class represents the renderable world and its contents.
	class Scene
	{
	public:
		Scene() = default;
		Scene(SceneParameters sceneParameters, const std::vector<SceneNode>& nodes);

	private:
		SceneParameters m_sceneParameters = SceneParameters{};
		std::vector<SceneNode> m_nodes = {};
	};
}
