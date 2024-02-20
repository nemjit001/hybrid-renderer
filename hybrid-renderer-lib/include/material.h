#pragma once

#include "platform.h"
#include "hri_math.h"

namespace hri
{
	/// @brief The Material struct describes drawing parameters for renderable objects.
	struct Material
	{
		ALIGNAS(16) Float3 diffuse			= Float3(0.8f);
		ALIGNAS(16) Float3 specular			= Float3(0.5f);
		ALIGNAS(16) Float3 transmittance	= Float3(0.0f);
		ALIGNAS(16) Float3 emission			= Float3(0.0f);
		ALIGNAS(4)  float shininess			= 1.0f;
		ALIGNAS(4)  float ior				= 1.45f;
	};
}