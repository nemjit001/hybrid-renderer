#pragma once

#include <hybrid_renderer.h>

struct MaterialParameters
{
	hri::Float3 diffuse			= hri::Float3(0.8f);
	hri::Float3 specular		= hri::Float3(0.5f);
	hri::Float3 transmittance	= hri::Float3(0.0f);
	hri::Float3 emission		= hri::Float3(0.0f);
	float shininess				= 1.0f;
	float ior					= 1.45f;
};

struct Material
{
	MaterialParameters parameters	= MaterialParameters{};
	hri::Texture albedoMap;
	hri::Texture normalMap;
};
