#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../shader_common.glsl"
#include "../raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT IlluminationRayPayload diPayload;

layout(set = 0, binding = 1, scalar) buffer RENDER_INSTANCE_DATA { RenderInstanceData instances[]; };
layout(set = 0, binding = 2) buffer MATERIAL_DATA { Material materials[]; };

void main()
{
	RenderInstanceData hitInstance = instances[gl_InstanceCustomIndexEXT];
	Material hitMaterial = materials[hitInstance.materialIdx];

	// only check if light is hit
	if (luminance(hitMaterial.emission) > 0.0)
	{
		diPayload.energy += diPayload.transmission * hitMaterial.emission;
	}
}
