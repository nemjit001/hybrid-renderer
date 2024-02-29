#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

#define RAYTRACE_RANGE_TMIN 1e-5
#define RAYTRACE_RANGE_TMAX 1e30

#define RT_PI		3.14159265358979323846264
#define RT_2PI		6.28318530717958647692528
#define RT_INV_PI	0.31830988618379067153777
#define RT_INV_2PI	0.15915494309189533576888

/// Shared include file for raytracing shader definitions

// RayHitPayload contains ray payload data.
struct RayHitPayload
{
	uint seed;
	float shadow;
	vec3 energy;
	vec3 transmission;
};

/// --- Random noise functions

uint WangHash(uint seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);

	return seed;
}

uint initSeed(uint seed)
{
	return WangHash((seed + 1) * 0x11);
}

uint randomUint(inout uint seed)
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

float randomFloat(inout uint seed)
{
	return randomUint(seed) * 2.3283064365387e-10;
}

float randomRange(inout uint seed, float minRange, float maxRange)
{
	return (randomFloat(seed) * (maxRange - minRange)) + minRange;
}

uint randomRangeU32(inout uint seed, uint minRange, uint maxRange)
{
	return (randomUint(seed) + minRange) % maxRange;
}

/// --- Random walk functions

vec3 diffuseReflect(inout uint seed, vec3 N)
{
	vec3 direction = vec3(0);

	do
	{
		direction = vec3(
			randomRange(seed, -1.0, 1.0),
			randomRange(seed, -1.0, 1.0),
			randomRange(seed, -1.0, 1.0)
		);
	} while (dot(direction, direction) > 1.0);

	if (dot(direction, N) < 0.0)
		direction *= -1.0;

	return normalize(direction);
}

vec3 randomWalk(inout uint seed, vec3 N)
{
	return diffuseReflect(seed, N);
}

/// --- Material evaluation functions

bool isEmissive(vec3 emission)
{
	return emission.x > 0.0 || emission.y > 0.0 || emission.z > 0;
}

float evaluatePDF(vec3 wO, vec3 N)
{
	return dot(wO, N) / RT_2PI;
}

vec3 evaluateBRDF(vec3 wI, vec3 wO, vec3 N, vec3 diffuse)
{
	return diffuse * RT_INV_PI;
}

#endif // RT_COMMON_GLSL
