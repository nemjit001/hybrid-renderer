#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../shader_common.glsl"
#include "../raytracing_common.glsl"

hitAttributeEXT vec2 triangleHitCoords;
layout(location = 0) rayPayloadInEXT PTRayPayload prd;

layout(buffer_reference, scalar) buffer VertexBuffer  { Vertex v[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { ivec3 i[]; };

layout(set = 0, binding = 1, scalar) buffer RENDER_INSTANCE_DATA { RenderInstanceData instances[]; };
layout(set = 0, binding = 2) buffer MATERIAL_DATA { Material materials[]; };

layout(set = 1, binding = 0) uniform accelerationStructureEXT TLAS;

void main()
{
	if (prd.traceDepth > RAYTRACE_MAX_BOUNCE_COUNT)
		return;

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

	// Calculate world space hit data
	vec3 wPos = vec3(gl_ObjectToWorldEXT * vec4(hitPos, 1));
	vec3 wNormal = vec3(gl_ObjectToWorldEXT * vec4(hitNormal, 0));
	vec3 Wi = gl_WorldRayDirectionEXT;

	wNormal = normalize(wNormal);
	if (dot(Wi, wNormal) > 0.0)
		wNormal *= -1.0;

	// Evaluate material (simple diffuse brdf)
	if (luminance(material.emission) > 0.0)
	{
		prd.energy += prd.transmission * material.emission;
		return;
	}

	bool specularEvent = false;
	vec3 Wo = vec3(0);
	randomWalk(prd.seed, Wi, wNormal, material, Wo, specularEvent);

	float pdf = evaluatePDF(Wo, wNormal, material, specularEvent);
	vec3 brdf = evaluateBRDF(Wi, Wo, wNormal, material, specularEvent);
	prd.transmission *= pdf * brdf;

	prd.traceDepth++;
	traceRayEXT(
		TLAS,
		gl_RayFlagsOpaqueEXT,
		prd.rayMask,
		0, 0, 0,
		wPos,
		RAYTRACE_RANGE_TMIN,
		Wo,
		RAYTRACE_RANGE_TMAX,
		0
	);
}
