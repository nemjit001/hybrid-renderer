#pragma once

#include <cmath>

#include "platform.h"

namespace hri
{
	ALIGNAS(4)
	struct Float2
	{
	public:
		inline Float2() : x(0.0f), y(0.0f) {};
		inline Float2(float val) : x(val), y(val) {};
		inline Float2(float _x, float _y) : x(_x), y(_y) {};

	public:
		union
		{
			float xy[2];
			struct { float x, y; };
			struct { float u, v; };
		};
	};

	ALIGNAS(4)
	struct Float3
	{
	public:
		inline Float3() : x(0.0f), y(0.0f), z(0.0f) {};
		inline Float3(float val) : x(val), y(val), z(val) {};
		inline Float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {};

	public:
		union
		{
			float xyz[3];
			struct { float x, y, z; };
			struct { float r, g, b; };
		};
	};

	ALIGNAS(4)
	struct Float4
	{
	public:
		inline Float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {};
		inline Float4(float val) : x(val), y(val), z(val), w(val) {};
		inline Float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {};

	public:
		union
		{
			float xyzw[4];
			struct { float x, y, z, w; };
			struct { float r, g, b, a; };
		};
	};

	/// @brief Simple inverse square root function.
	/// @param val Value to calculate inverse sqrt for.
	/// @return The inverse square root of val.
	inline float rsqrtf(float val) { return 1.0f / sqrtf(val); }
}
