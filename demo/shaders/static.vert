#version 450

layout(location = 0) in vec3 VertexPosition;
layout(location = 1) in vec3 VertexNormal;
layout(location = 2) in vec3 VertexTangent;
layout(location = 3) in vec2 VertexTexCoord;

layout(location = 0) out VS_OUT
{
    vec4 wPos;
    vec3 normal;
    vec2 texCoord;
} vs_out;

layout(set = 0, binding = 0) uniform CAMERA
{
    vec3 position;
    mat4 view;
    mat4 project;
} camera;

layout(push_constant) uniform TRANSFORM
{
    mat4 model;
    mat3 normal;
} transform;

void main()
{
    vec4 wPos = transform.model * vec4(VertexPosition, 1);

    vs_out.wPos = wPos;
    vs_out.normal = normalize(transform.model * vec4(VertexNormal, 0)).xyz;
    vs_out.texCoord = VertexTexCoord;

    gl_Position = camera.project * camera.view * vs_out.wPos;
}
