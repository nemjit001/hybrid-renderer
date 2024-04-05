#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../shader_common.glsl"
#include "../raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT PTRayPayload prd;

void main()
{
	prd.hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * RAYTRACE_RANGE_TMAX;
	prd.hitNormal = -1.0 * gl_WorldRayDirectionEXT;

	prd.energy += prd.transmission * SKY_COLOR;
}
