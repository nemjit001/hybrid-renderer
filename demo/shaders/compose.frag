#version 450

layout(location = 0) in vec2 ScreenUV;

layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D SoftShadowResult;
layout(binding = 1) uniform sampler2D DIResult;

void main()
{
	vec3 DIColor = texture(DIResult, ScreenUV).rgb;

	FragColor = vec4(DIColor, 1);
}