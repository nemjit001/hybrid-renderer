#version 450

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shader_common.glsl"

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
    Camera camera;
};

layout(push_constant) uniform INSTANCE_PC
{
    uint instanceId;
    mat4 model;
    mat3 normal;
} push;

void main()
{
    vec4 wPos = push.model * vec4(VertexPosition, 1);

    vs_out.wPos = wPos;
    vs_out.normal = normalize(push.model * vec4(VertexNormal, 0)).xyz;
    vs_out.texCoord = VertexTexCoord;

    gl_Position = camera.project * camera.view * vs_out.wPos;
}
