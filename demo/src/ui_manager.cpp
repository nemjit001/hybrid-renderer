#include "ui_manager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "window_manager.h"

UIManager::UIManager(WindowHandle* window)
{
	IMGUI_CHECKVERSION();
	m_guiContext = ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO& guiIO = ImGui::GetIO();
	guiIO.IniFilename = nullptr;

	ImGui_ImplGlfw_InitForVulkan(window, true);
}

UIManager::~UIManager()
{
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext(m_guiContext);
}

void UIManager::startDraw() const
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void UIManager::endDraw() const
{
	ImGui::EndFrame();
	ImGui::Render();
}
