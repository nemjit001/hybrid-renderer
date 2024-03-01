#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT DIRayPayload diPayload;

void main()
{
	// TODO: fetch light data by sampled index & primitive
	diPayload.energy = diPayload.transmission * LIGHT_COLOR;
}
