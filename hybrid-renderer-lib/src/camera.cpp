#include "camera.h"

#include "hri_math.h"

using namespace hri;

Camera::Camera(CameraParameters parameters)
	:
	m_parameters(parameters)
{
	//
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
}

CameraShaderData Camera::getShaderData()
{
	return CameraShaderData{
		position,
		// view
		// project
	};
}
