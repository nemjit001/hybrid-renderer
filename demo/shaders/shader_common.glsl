#ifndef SHADER_COMMON_GLSL
#define SHADER_COMMON_GLSL

/// Shared include file for shader definitions

// Mirrors render instance data from scene.h
struct RenderInstanceData
{
	uint materialIdx;
	uint64_t vertexBufferAddress;
	uint64_t indexBufferAddress;
};

// Mirrors material data from material.h
struct Material
{
	vec3 diffuse;
	vec3 specular;
	vec3 transmittance;
	vec3 emission;
	float shininess;
	float ior;
};

// Mirrors vertex struct hri::Vertex
struct Vertex
{
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec2 textureCoord;
};

#endif // SHADER_COMMON_GLSL
