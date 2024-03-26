#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../shader_common.glsl"
#include "../raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT DIRayPayload prd;

layout(set = 1, binding = 3, scalar) buffer RENDER_INSTANCE_DATA { RenderInstanceData instances[]; };
layout(set = 1, binding = 4) buffer MATERIAL_DATA { Material materials[]; };

void main()
{
	RenderInstanceData light = instances[prd.lightInstanceID];
	Material mat = materials[light.materialIdx];

	prd.energy += prd.transmission * mat.emission;
}
