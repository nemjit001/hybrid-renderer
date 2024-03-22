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
	// Set up instance & buffer data
	RenderInstanceData hitInstance = instances[gl_InstanceCustomIndexEXT];
	Material material = materials[hitInstance.materialIdx];

	// If we hit a light, return color
	if (luminance(material.emission) > 0.0)
	{
		prd.energy += prd.transmission * material.emission;
	}
}
