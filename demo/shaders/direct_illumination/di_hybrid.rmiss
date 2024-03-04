#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT IlluminationRayPayload diPayload;

void main()
{
	diPayload.energy += diPayload.transmission * SKY_COLOR;
}
