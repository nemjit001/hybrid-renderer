#version 450

layout(location = 0) in vec2 ScreenUV;

layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D RenderResult;

void main()
{
	FragColor = texture(RenderResult, ScreenUV);
}