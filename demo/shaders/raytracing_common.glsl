#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

#define RAYTRACE_RANGE_TMIN 1e-5
#define RAYTRACE_RANGE_TMAX 1000.0

/// Shared include file for raytracing shader definitions
struct RayHitPayload
{
	vec3 hitValue;
};

#endif // RT_COMMON_GLSL
