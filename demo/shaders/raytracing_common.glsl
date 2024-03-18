#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

#define RAYTRACE_MAX_BOUNCE_COUNT	5
#define RAYTRACE_RANGE_TMIN			1e-3
#define RAYTRACE_RANGE_TMAX			1e30
#define RAYTRACE_MASK_BITS			8

#define REPROJECT_PLANE_DIST		1e-2
#define REPROJECT_DELTA_THRESHOLD	1e-2

#define RT_PI		3.14159265358979323846264
#define RT_2PI		6.28318530717958647692528
#define RT_INV_PI	0.31830988618379067153777
#define RT_INV_2PI	0.15915494309189533576888

#define SKY_COLOR	vec3(0.7, 0.85, 0.95)

#include "rand.glsl"
#include "shader_common.glsl"

/// Shared include file for raytracing shader definitions

struct FrameInfo
{
	uint frameIndex;
};

struct HybridInitialHit
{
	bool miss;
	vec3 worldPos;
	vec3 worldNormal;
	vec3 Wi;
};

struct PTRayPayload
{
	uint seed;
	uint traceDepth;
	uint rayMask;
	vec3 hitPos;
	vec3 hitNormal;
	vec3 energy;
	vec3 transmission;
};

/// --- Common functions

uint calculateLaunchIndex(uvec3 launchID, uvec3 launchSize)
{
	return launchID.x + launchID.y * launchSize.x + launchID.z * launchSize.x * launchSize.y;
}

bool gbufferRayHit(float depth)
{
	return depth < 1.0;
}

HybridInitialHit getInitialHitData(mat4 inverseView, mat4 inverseProject, vec2 inUV, sampler2D GBufferNormal, sampler2D GBufferDepth)
{
	vec2 ndc = 2.0 * inUV - 1.0;

	float initialHitDepth = float(texture(GBufferDepth, inUV));
	vec3 wPos = depthToWorldPos(inverseProject, inverseView, ndc, initialHitDepth);

	vec3 wNormal = vec3(texture(GBufferNormal, inUV));
	vec3 Wi = vec3(inverseView * vec4(normalize((inverseProject * vec4(ndc, 1, 1)).xyz), 0));

	wNormal = normalize(wNormal);
	if (dot(Wi, wNormal) > 0.0) wNormal *= -1.0;

	HybridInitialHit hit;
	hit.miss = !gbufferRayHit(initialHitDepth);
	hit.worldPos = wPos;
	hit.worldNormal = wNormal;
	hit.Wi = Wi;

	return hit;
}

Material getMaterialFromGBuffer(
	vec2 inUV,
	sampler2D GBufferAlbedo,
	sampler2D GBufferMatSpecular,
	sampler2D GBufferMatTransmittance,
	sampler2D GBufferEmission
)
{
	vec4 specularShininess = texture(GBufferMatSpecular, inUV);
	vec4 transmittanceIOR = texture(GBufferMatTransmittance, inUV);
	Material material = Material(
		vec3(texture(GBufferAlbedo, inUV)),		// diffuse color
		specularShininess.rgb,					// specular color
		transmittanceIOR.rgb,					// transmittance
		vec3(texture(GBufferEmission, inUV)),	// emission color & strength
		specularShininess.a,					// shininess
		transmittanceIOR.a						// IOR
	);

	return material;
}

/// --- Reproject functions

bool uvWithinRange(vec2 UV)
{
	return UV.x >= 0.0 && UV.x <= 1.0 && UV.y >= 0.0 && UV.y <= 1.0;
}

float camNearPlane(Camera cam)
{
// TODO: calculate this correctly pls
	return 2.5;
}

vec3 screenToWorldSpace(Camera cam, vec2 uv, float depth)
{
	vec3 dir = normalize(cam.forward * camNearPlane(cam) + cam.right * uv.x + cam.up * uv.y);
	return cam.position + dir * depth;
}

vec2 worldToScreenSpace(Camera cam, vec3 worldPos)
{
	vec3 camToPos = worldPos - cam.position;
    vec3 camToPosNorm = normalize(camToPos);
    
	float nearPlane = camNearPlane(cam);
	vec3 forward = cam.forward * nearPlane;
    float d = dot(cam.forward, camToPosNorm);
    if(d < 1e-3)
        return vec2(0);
    
    d = nearPlane / d;
    camToPos = camToPosNorm * d - forward;
    
    float x = dot(camToPos, cam.right);
    float y = dot(camToPos, cam.up);
    return vec2(x, y);
}

vec2 calculateMotionVector(Camera prevCamera, Camera currCamera, vec3 worldPos, float aspectRatio)
{
	vec2 prevPos = worldToScreenSpace(prevCamera, worldPos);
	prevPos.x /= aspectRatio;

	vec2 currPos = worldToScreenSpace(currCamera, worldPos);	
	currPos.x /= aspectRatio;

	return prevPos - currPos;
}

// Simple plane distance reproject check based on https://diharaw.github.io/post/adventures_in_hybrid_rendering/
bool reprojectValid(vec3 currPos, vec3 prevPos, vec3 currNormal)
{
	vec3 prevToCur = currPos - prevPos;
	float planeDistance = abs(dot(prevToCur, currNormal));

	return planeDistance < REPROJECT_PLANE_DIST || length(prevToCur) < REPROJECT_DELTA_THRESHOLD;
}

/// --- Random walk functions

vec3 diffuseReflect(inout uint seed, vec3 wI, vec3 N)
{
	vec3 outDir = vec3(0);
	do
	{
		outDir = vec3(
			randomRange(seed, -1.0, 1.0),
			randomRange(seed, -1.0, 1.0),
			randomRange(seed, -1.0, 1.0)
		);
	} while(dot(outDir, outDir) > 1.0);

	if (dot(outDir, N) < 0.0)
		outDir *= -1.0;

	return normalize(outDir);
}

void randomWalk(inout uint seed, vec3 Wi, vec3 N, Material material, out vec3 Wo, out bool specularEvent)
{
	float rng = randomFloat(seed);

// TODO: uncomment when frame accumulation works
//	if (rng < material.specular.r || rng < material.specular.g || rng < material.specular.b)
//	{
//		specularEvent = true;
//		Wo = reflect(Wi, N);
//		return;
//	}

	specularEvent = false;
	Wo = diffuseReflect(seed, Wi, N);
}

/// --- Material evaluation functions

float luminance(vec3 color)
{
	return color.r + color.g + color.b;
}

float evaluatePDF(vec3 Wo, vec3 N, Material material, bool specularEvent)
{
	if (specularEvent)
		return 1.0;

	return dot(Wo, N) / RT_2PI;
}

vec3 evaluateBRDF(vec3 Wi, vec3 Wo, vec3 N, Material material, bool specularEvent)
{
	if (specularEvent)
		return material.diffuse * material.specular;

	return material.diffuse * RT_INV_PI;
}

#endif // RT_COMMON_GLSL
