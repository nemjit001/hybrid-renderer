#include "camera.h"

#include "hri_math.h"

using namespace hri;

Camera::Camera(CameraParameters parameters)
	:
	m_parameters(parameters)
{
	updateMatrices();
}

Camera::Camera(CameraParameters parameters, const Float3& position, const Float3& target)
	:
	position(position),
	forward(normalize(target - position)),
	m_parameters(parameters)
{
	Float3 _right = normalize(cross(HRI_WORLD_UP, forward));

	up = normalize(cross(forward, _right));
	right = normalize(cross(up, forward));

	updateMatrices();
}

void Camera::updateMatrices()
{
	view = lookAt(static_cast<glm::vec3>(position), static_cast<glm::vec3>(position + forward), static_cast<glm::vec3>(up));
	project = perspective(radians(m_parameters.fovYDegrees), m_parameters.aspectRatio, m_parameters.zNear, m_parameters.zFar);

	project[1][1] *= -1;	// Flip Y to account for viewport flip
}

CameraShaderData Camera::getShaderData()
{
	return CameraShaderData{
		position,
		view,
		project,
	};
}
