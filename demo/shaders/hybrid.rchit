#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "shader_common.glsl"
#include "raytracing_common.glsl"

hitAttributeEXT vec2 triangleHitCoords;
layout(location = 0) rayPayloadInEXT RayHitPayload prd;

layout(buffer_reference, scalar) buffer VertexBuffer  { Vertex v[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { ivec3 i[]; };

layout(set = 0, binding = 1, scalar) buffer RENDER_INSTANCE_DATA { RenderInstanceData instances[]; };
layout(set = 0, binding = 2) buffer MATERIAL_DATA { Material materials[]; };

layout(set = 1, binding = 0) uniform accelerationStructureEXT TLAS;

void main()
{
	const uint rayFlags = gl_RayFlagsOpaqueEXT;
	const uint cullMask = 0xFF;

	// Set up instance & buffer data
	RenderInstanceData hitInstance = instances[gl_InstanceCustomIndexEXT];
	Material material = materials[hitInstance.materialIdx];
	VertexBuffer vertices = VertexBuffer(hitInstance.vertexBufferAddress);
	IndexBuffer indices = IndexBuffer(hitInstance.indexBufferAddress);

	// Get hit vertex data
	const ivec3 triangle = indices.i[gl_PrimitiveID];
	const Vertex v0 = vertices.v[triangle.x];
	const Vertex v1 = vertices.v[triangle.y];
	const Vertex v2 = vertices.v[triangle.z];

	// Calculate hit position & normal
	const vec3 barycentric = vec3(1.0 - triangleHitCoords.x - triangleHitCoords.y, triangleHitCoords.x, triangleHitCoords.y);
	vec3 hitPos = barycentric.x * v0.position + barycentric.y * v1.position + barycentric.z * v2.position;
	vec3 hitNormal = barycentric.x * v0.normal + barycentric.y * v1.normal + barycentric.z * v2.normal;

	// Calculate world positions
	vec3 wPos = vec3(gl_ObjectToWorldEXT * vec4(hitPos, 1));
	vec3 wNormal = normalize(vec3(gl_ObjectToWorldEXT * vec4(hitNormal, 0)));
	vec3 wInDir = gl_WorldRayDirectionEXT;

	// Evaluate hit material
	if (isEmissive(material.emission))
	{
		prd.terminated = true;
		prd.energy += prd.transmission * material.emission;
		return;
	}

	// Flip normal to account for backface hits
	if (dot(wNormal, wInDir) > 0.0)
		wNormal *= -1.0;

	vec3 wOutDir = randomWalk(prd.seed, wInDir, wNormal);
	float pdf = evaluatePDF(wOutDir, wNormal);
	vec3 brdf = evaluateBRDF(wInDir, wOutDir, wNormal, material.diffuse);
	prd.transmission *= pdf * brdf;

	prd.origin = wPos;
	prd.direction = wOutDir;
}
