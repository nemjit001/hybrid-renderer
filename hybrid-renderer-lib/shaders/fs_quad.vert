#version 450

layout(location = 0) out vec2 ScreenUV;

void main()
{
	ScreenUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(2.0 * ScreenUV - 1.0, 0.0, 1.0);
}
