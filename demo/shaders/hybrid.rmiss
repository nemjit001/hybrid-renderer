#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shader_common.glsl"
#include "raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT RayHitPayload prd;

vec3 sampleSkyColor(vec3 rayDirection)
{
	vec3 colorA = vec3(0.8, 0.8, 0.8);
	vec3 colorB = vec3(0.1, 0.4, 0.6);
	float alpha = rayDirection.y;

	//return alpha * colorB + (1.0 - alpha) * colorA;
	return vec3(0);
}

void main()
{
	// TODO: sample environment map
	prd.shadow = 0.0;
	prd.energy += prd.transmission * sampleSkyColor(gl_WorldRayDirectionEXT);
}
