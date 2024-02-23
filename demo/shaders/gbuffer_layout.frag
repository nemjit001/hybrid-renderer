#version 450

layout(location = 0) in VS_OUT
{
    vec4 wPos;
    vec3 normal;
    vec2 texCoord;
} fs_in;

layout(location = 0) out vec4 FragAlbedo;
layout(location = 1) out vec4 FragWPos;
layout(location = 2) out vec4 FragNormal;

void main()
{
    FragAlbedo = vec4(fs_in.texCoord, 0, 1);
    FragWPos = vec4(fs_in.wPos.xyz / fs_in.wPos.w, 1);
    FragNormal = vec4(fs_in.normal, 1);
}
