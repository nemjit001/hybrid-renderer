#ifndef SHADER_COMMON_GLSL
#define SHADER_COMMON_GLSL

/// Shared include file for shader definitions

// Mirrors render instance data from scene.h
struct RenderInstanceData
{
	uint materialIdx;
	uint indexCount;
	uint64_t vertexBufferAddress;
	uint64_t indexBufferAddress;
};

struct LighArrayEntry
{
	uint lightIdx;
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

// Mirrors hri::CameraShaderData
struct Camera
{
	vec3 position;
    mat4 view;
    mat4 project;
};

vec3 depthToWorldPos(mat4 invProject, mat4 invView, vec2 ndc, float depth)
{
	vec4 clip = invProject * vec4(ndc, depth, 1);
	vec4 view = invView * vec4(clip.xyz / clip.w, 1.0);

	return view.xyz;
}

#endif // SHADER_COMMON_GLSL
