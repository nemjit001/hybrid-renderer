#version 450

#extension GL_EXT_scalar_block_layout : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "shader_common.glsl"

layout(location = 0) in vec2 ScreenUV;

layout(set = 0, binding = 0) uniform sampler2D RNGSource;

layout(set = 1, binding = 0) uniform sampler2D GBufferLoAlbedo;
layout(set = 1, binding = 1) uniform sampler2D GBufferLoEmission;
layout(set = 1, binding = 2) uniform sampler2D GBufferLoSpecular;
layout(set = 1, binding = 3) uniform sampler2D GBufferLoTransmittance;
layout(set = 1, binding = 4) uniform sampler2D GBufferLoNormal;
layout(set = 1, binding = 5) uniform sampler2D GBufferLoLODMask;
layout(set = 1, binding = 6) uniform sampler2D GBufferLoDepth;

layout(set = 2, binding = 0) uniform sampler2D GBufferHiAlbedo;
layout(set = 2, binding = 1) uniform sampler2D GBufferHiEmission;
layout(set = 2, binding = 2) uniform sampler2D GBufferHiSpecular;
layout(set = 2, binding = 3) uniform sampler2D GBufferHiTransmittance;
layout(set = 2, binding = 4) uniform sampler2D GBufferHiNormal;
layout(set = 2, binding = 5) uniform sampler2D GBufferHiLODMask;
layout(set = 2, binding = 6) uniform sampler2D GBufferHiDepth;

layout(location = 0) out vec4 FragAlbedo;
layout(location = 1) out vec4 FragEmission;
layout(location = 2) out vec4 FragSpecular;
layout(location = 3) out vec4 FragTransmittance;
layout(location = 4) out vec4 FragNormal;
layout(location = 5) out float FragDepth;

layout(push_constant) uniform IMAGE_INFO { vec2 resolution; };

void main()
{
	uint rng = uint(texture(RNGSource, ScreenUV).r);
	uint LODLoDefMask = uint(texture(GBufferLoLODMask, ScreenUV).r);
	uint LODHiDefMask = uint(texture(GBufferHiLODMask, ScreenUV).r);
	uint rayMask = generateRayMask(rng);

	// Set default values
	FragAlbedo = vec4(0);
	FragEmission = vec4(0);
	FragSpecular = vec4(0);
	FragTransmittance = vec4(0);
	FragNormal = vec4(0);
	FragDepth = 1.0;

	if ((LODLoDefMask & rayMask) != 0)	// TODO: check how to blend between 2 layers so that no parts are missing
	{
		FragAlbedo = texture(GBufferLoAlbedo, ScreenUV);
		FragEmission = texture(GBufferLoEmission, ScreenUV);
		FragSpecular = texture(GBufferLoSpecular, ScreenUV);
		FragTransmittance = texture(GBufferLoTransmittance, ScreenUV);
		FragNormal = texture(GBufferLoNormal, ScreenUV);
		FragDepth = texture(GBufferLoDepth, ScreenUV).r;
	}
	else
	{
		FragAlbedo = texture(GBufferHiAlbedo, ScreenUV);
		FragEmission = texture(GBufferHiEmission, ScreenUV);
		FragSpecular = texture(GBufferHiSpecular, ScreenUV);
		FragTransmittance = texture(GBufferHiTransmittance, ScreenUV);
		FragNormal = texture(GBufferHiNormal, ScreenUV);
		FragDepth = texture(GBufferHiDepth, ScreenUV).r;
	}
}