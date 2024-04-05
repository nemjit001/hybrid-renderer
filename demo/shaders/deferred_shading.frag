#version 450

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "raytracing_common.glsl"

layout(location = 0) in vec2 ScreenUV;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D GBufferAlbedo;
layout(set = 0, binding = 1) uniform sampler2D GBufferEmission;
layout(set = 0, binding = 2) uniform sampler2D GBufferSpecular;
layout(set = 0, binding = 3) uniform sampler2D GBufferTransmittance;
layout(set = 0, binding = 4) uniform sampler2D GBufferNormal;
layout(set = 0, binding = 5) uniform sampler2D GBufferDepth;
layout(set = 0, binding = 6) uniform sampler2D DirectIllumination;

void main()
{
// TODO: PBR deferred shading using DI & GBuffer data
	vec3 DI = texture(DirectIllumination, ScreenUV).rgb;	// DI should be resolved BRDF
	vec3 albedo = texture(GBufferAlbedo, ScreenUV).rgb;
	vec3 emission = texture(GBufferEmission, ScreenUV).rgb;
	float depth = texture(GBufferDepth, ScreenUV).r;

	vec3 outColor = vec3(1);
	if (luminance(emission) > 0.0)
	{	// Hit light -> overlay light on shading pass
		outColor = emission;
	}
	else if (depth == 1.0)
	{	// Missed scene entirely -> draw sky
		outColor = SKY_COLOR;
	}
	else
	{	// Hit something, resolve Direct Illumination
		outColor = albedo * DI;
	}

	FragColor = vec4(outColor, 1);
}