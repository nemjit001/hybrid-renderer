#pragma once

#include "hri_math.h"

#define HRI_WORLD_FORWARD	hri::Float3(0.0f, 0.0f, 1.0f)
#define HRI_WORLD_UP		hri::Float3(0.0f, 1.0f, 0.0f)
#define HRI_WORLD_RIGHT		hri::Float3(1.0f, 0.0f, 0.0f)

namespace hri
{
	/// @brief The camera parameters struct contains parameters for the projection matrix and viewplane generation.
	struct CameraParameters
	{
		float fovYDegrees	= 60.0f;
	};

	/// @brief Shader layout of camera data
	struct CameraShaderData
	{
		ALIGNAS(16) Float3 position;
		ALIGNAS(16) Float4x4 view;
		ALIGNAS(16) Float4x4 project;
	};

	/// @brief A virtual camera used in rendering operations.
	class Camera
	{
	public:
		/// @brief Instantiate a camera with default parameters at the world origin looking forward.
		Camera() = default;

		/// @brief Instantiate a camera at the world origin, looking forward, with custom parameters.
		/// @param parameters Camera parameters to use.
		Camera(CameraParameters parameters);

		/// @brief Instantiate a camera.
		/// @param parameters Camera parameters.
		/// @param position World position of the camera.
		/// @param target Target where the camera is looking towards. MUST NOT be directly down or up.
		Camera(CameraParameters parameters, const Float3& position, const Float3& target);

		/// @brief Destroy this camera instance.
		virtual ~Camera() = default;

		/// @brief Retrieve the camera data in a shader ready layout.
		/// @return A Camera Shader Data struct.
		CameraShaderData getShaderData();

	public:
		Float3 position		= Float3(0.0f);
		Float3 forward		= HRI_WORLD_FORWARD;
		Float3 right		= HRI_WORLD_RIGHT;
		Float3 up			= HRI_WORLD_UP;
		Float4x4 view		= Float4x4(0.0f);
		Float4x4 project	= Float4x4(0.0f);
	
	private:
		CameraParameters m_parameters = CameraParameters{};
	};
}
