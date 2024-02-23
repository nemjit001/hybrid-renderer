#pragma once

#include <hybrid_renderer.h>

struct MaterialParameters
{
	HRI_ALIGNAS(16) hri::Float3 diffuse			= hri::Float3(0.8f);
	HRI_ALIGNAS(16) hri::Float3 specular		= hri::Float3(0.5f);
	HRI_ALIGNAS(16) hri::Float3 transmittance	= hri::Float3(0.0f);
	HRI_ALIGNAS(16) hri::Float3 emission		= hri::Float3(0.0f);
	HRI_ALIGNAS(4)  float shininess				= 1.0f;
	HRI_ALIGNAS(4)  float ior					= 1.45f;
};

struct Material
{
	MaterialParameters parameters	= MaterialParameters{};
	hri::Texture albedoMap;
	hri::Texture normalMap;
};
