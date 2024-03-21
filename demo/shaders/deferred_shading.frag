#version 450

layout(location = 0) in vec2 ScreenUV;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D GBufferAlbedo;
layout(set = 0, binding = 1) uniform sampler2D GBufferEmission;
layout(set = 0, binding = 2) uniform sampler2D GBufferSpecular;
layout(set = 0, binding = 3) uniform sampler2D GBufferTransmittance;
layout(set = 0, binding = 4) uniform sampler2D GBufferNormal;

void main()
{
// TODO: PBR deferred shading using DI & GBuffer data
	vec3 albedo = texture(GBufferAlbedo, ScreenUV).rgb;
	vec3 normal = texture(GBufferNormal, ScreenUV).rgb;

	vec3 L = normalize(vec3(0, 0.5, -1.0));

	FragColor = vec4(albedo * dot(normal, L), 1);
}