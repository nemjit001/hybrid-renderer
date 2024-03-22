#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../shader_common.glsl"
#include "../raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT DIRayPayload prd;

void main()
{
	prd.energy += prd.transmission * SKY_COLOR;
}
