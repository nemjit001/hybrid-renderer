#version 450

layout(location = 0) in vec2 ScreenUV;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D GBufferAlbedo;
layout(set = 0, binding = 1) uniform sampler2D GBufferEmission;
layout(set = 0, binding = 2) uniform sampler2D GBufferSpecular;
layout(set = 0, binding = 3) uniform sampler2D GBufferTransmittance;
layout(set = 0, binding = 4) uniform sampler2D GBufferNormal;
layout(set = 0, binding = 5) uniform sampler2D DirectIllumination;

void main()
{
// TODO: PBR deferred shading using DI & GBuffer data
	vec3 DI = texture(DirectIllumination, ScreenUV).rgb;	// DI should be resolved BRDF without albedo
	vec3 albedo = texture(GBufferAlbedo, ScreenUV).rgb;

	vec3 outColor = albedo * DI;
	FragColor = vec4(outColor, 1);
}