#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "window_manager.h"

/// @brief The UI Manager class handles UI initialization & context.
class UIManager
{
public:
	/// @brief Create a new UI Manager.
	/// @param window Window handle to use.
	UIManager(WindowHandle* window);

	/// @brief Destroy this class instance.
	~UIManager();

	/// @brief Start drawing UI.
	void startDraw() const;

	/// @brief End UI drawing.
	void endDraw() const;

private:
	ImGuiContext* m_guiContext;
};
