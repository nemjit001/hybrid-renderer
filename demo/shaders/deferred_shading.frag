#version 450

layout(location = 0) in vec2 ScreenUV;

layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D GBufferAlbedo;
layout(binding = 1) uniform sampler2D GBufferEmission;
layout(binding = 2) uniform sampler2D GBufferSpecular;
layout(binding = 3) uniform sampler2D GBufferTransmittance;
layout(binding = 4) uniform sampler2D GBufferNormal;
layout(binding = 5) uniform sampler2D DIResult;

void main()
{
	FragColor = vec4(texture(DIResult, ScreenUV).rgb, 1);
}