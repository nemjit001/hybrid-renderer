#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shader_common.glsl"
#include "raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT RayHitPayload prd;

void main()
{
	prd.energy += prd.transmission * sampleSkyColor(gl_WorldRayDirectionEXT);
}
