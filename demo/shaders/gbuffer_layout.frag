#version 450

#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "shader_common.glsl"

layout(location = 0) in VS_OUT
{
    vec4 wPos;
    vec3 normal;
    vec2 texCoord;
} fs_in;

layout(location = 0) out vec4 FragAlbedo;
layout(location = 1) out vec4 FragEmission;
layout(location = 2) out vec4 FragWPos;
layout(location = 3) out vec4 FragNormal;

layout(push_constant) uniform INSTANCE_PC
{
    uint instanceId;
    mat4 model;
    mat3 normal;
} push;

layout(buffer_reference, scalar) buffer VERTEX_DATA { Vertex vertices[]; };
layout(buffer_reference, scalar) buffer INDEX_DATA { uint indices[]; };

layout(set = 0, binding = 1, scalar) buffer RENDER_INSTANCE_DATA { RenderInstanceData instances[]; };
layout(set = 0, binding = 2) buffer MATERIAL_DATA { Material materials[]; };

void main()
{
    RenderInstanceData instance = instances[push.instanceId];
    Material material = materials[instance.materialIdx];

    FragAlbedo = vec4(material.diffuse, 1);
    FragEmission = vec4(material.emission, 1);
    FragWPos = vec4(fs_in.wPos.xyz / fs_in.wPos.w, 1);
    FragNormal = vec4(normalize(fs_in.normal), 1);
}
