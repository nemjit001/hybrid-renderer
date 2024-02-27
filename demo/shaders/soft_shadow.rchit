#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "shader_common.glsl"
#include "raytracing_common.glsl"

layout(location = 0) rayPayloadInEXT RayHitPayload prd;

layout(buffer_reference, scalar) buffer VERTEX_DATA { Vertex vertices[]; };
layout(buffer_reference, scalar) buffer INDEX_DATA { uint indices[]; };

layout(set = 0, binding = 1, scalar) buffer RENDER_INSTANCE_DATA { RenderInstanceData instances[]; };
layout(set = 0, binding = 2) buffer MATERIAL_DATA { Material materials[]; };

layout(set = 1, binding = 0) uniform accelerationStructureEXT TLAS;

void main()
{
	// TODO: random bounce or random sample of lights in scene
}
