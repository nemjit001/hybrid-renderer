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
    mat4 viewProject;
} camera;

layout(set = 1, binding = 0) uniform MODEL
{
    mat4 transform;
} model;

void main()
{
    vec4 wPos = model.transform * vec4(VertexPosition, 1);

    vs_out.wPos = wPos;
    vs_out.normal = normalize(VertexNormal);
    vs_out.texCoord = VertexTexCoord;

    gl_Position = camera.viewProject * vs_out.wPos;
}
