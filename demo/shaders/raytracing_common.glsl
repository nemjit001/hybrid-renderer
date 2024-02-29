#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

#define RAYTRACE_RANGE_TMIN 1e-5
#define RAYTRACE_RANGE_TMAX 10000.0

/// Shared include file for raytracing shader definitions

// RayHitPayload contains ray payload data.
struct RayHitPayload
{
	float shadow;
};

vec3 randomWalk(vec3 N)
{
	return N;
}

#endif // RT_COMMON_GLSL
