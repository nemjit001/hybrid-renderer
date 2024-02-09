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

	// --- Standard math operations

	inline Float2 operator+(const Float2& a, const Float2& b) { return Float2(a.x + b.x, a.y + b.y); }
	inline Float3 operator+(const Float3& a, const Float3& b) { return Float3(a.x + b.x, a.y + b.y, a.z + b.z); }
	inline Float4 operator+(const Float4& a, const Float4& b) { return Float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }

	inline Float2 operator-(const Float2& a, const Float2& b) { return Float2(a.x - b.x, a.y - b.y); }
	inline Float3 operator-(const Float3& a, const Float3& b) { return Float3(a.x - b.x, a.y - b.y, a.z - b.z); }
	inline Float4 operator-(const Float4& a, const Float4& b) { return Float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }

	inline Float2 operator*(const Float2& a, const Float2& b) { return Float2(a.x * b.x, a.y * b.y); }
	inline Float3 operator*(const Float3& a, const Float3& b) { return Float3(a.x * b.x, a.y * b.y, a.z * b.z); }
	inline Float4 operator*(const Float4& a, const Float4& b) { return Float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }

	inline Float2 operator/(const Float2& a, const Float2& b) { return Float2(a.x / b.x, a.y / b.y); }
	inline Float3 operator/(const Float3& a, const Float3& b) { return Float3(a.x / b.x, a.y / b.y, a.z / b.z); }
	inline Float4 operator/(const Float4& a, const Float4& b) { return Float4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }

	inline Float2 operator+(float a, const Float2& b) { return Float2(a + b.x, a + b.y); }
	inline Float3 operator+(float a, const Float3& b) { return Float3(a + b.x, a + b.y, a + b.z); }
	inline Float4 operator+(float a, const Float4& b) { return Float4(a + b.x, a + b.y, a + b.z, a + b.w); }

	inline Float2 operator-(float a, const Float2& b) { return Float2(a - b.x, a - b.y); }
	inline Float3 operator-(float a, const Float3& b) { return Float3(a - b.x, a - b.y, a - b.z); }
	inline Float4 operator-(float a, const Float4& b) { return Float4(a - b.x, a - b.y, a - b.z, a - b.w); }

	inline Float2 operator*(float a, const Float2& b) { return Float2(a * b.x, a * b.y); }
	inline Float3 operator*(float a, const Float3& b) { return Float3(a * b.x, a * b.y, a * b.z); }
	inline Float4 operator*(float a, const Float4& b) { return Float4(a * b.x, a * b.y, a * b.z, a * b.w); }

	inline Float2 operator/(float a, const Float2& b) { return Float2(a / b.x, a / b.y); }
	inline Float3 operator/(float a, const Float3& b) { return Float3(a / b.x, a / b.y, a / b.z); }
	inline Float4 operator/(float a, const Float4& b) { return Float4(a / b.x, a / b.y, a / b.z, a / b.w); }

	inline Float2 operator+(const Float2& a, float b) { return Float2(a.x + b, a.y + b); }
	inline Float3 operator+(const Float3& a, float b) { return Float3(a.x + b, a.y + b, a.z + b); }
	inline Float4 operator+(const Float4& a, float b) { return Float4(a.x + b, a.y + b, a.z + b, a.w + b); }

	inline Float2 operator-(const Float2& a, float b) { return Float2(a.x - b, a.y - b); }
	inline Float3 operator-(const Float3& a, float b) { return Float3(a.x - b, a.y - b, a.z - b); }
	inline Float4 operator-(const Float4& a, float b) { return Float4(a.x - b, a.y - b, a.z - b, a.w - b); }

	inline Float2 operator*(const Float2& a, float b) { return Float2(a.x * b, a.y * b); }
	inline Float3 operator*(const Float3& a, float b) { return Float3(a.x * b, a.y * b, a.z * b); }
	inline Float4 operator*(const Float4& a, float b) { return Float4(a.x * b, a.y * b, a.z * b, a.w * b); }

	inline Float2 operator/(const Float2& a, float b) { return Float2(a.x / b, a.y / b); }
	inline Float3 operator/(const Float3& a, float b) { return Float3(a.x / b, a.y / b, a.z / b); }
	inline Float4 operator/(const Float4& a, float b) { return Float4(a.x / b, a.y / b, a.z / b, a.w / b); }

	inline Float2 operator-(const Float2& vec) { return -1.0f * vec; }
	inline Float3 operator-(const Float3& vec) { return -1.0f * vec; }
	inline Float4 operator-(const Float4& vec) { return -1.0f * vec; }

	// --- Assignment math operations

	inline void operator+=(Float2& a, const Float2& b) { a.x += b.x; a.y += b.y; }
	inline void operator+=(Float3& a, const Float3& b) { a.x += b.x; a.y += b.y; a.z += b.z; }
	inline void operator+=(Float4& a, const Float4& b) { a.x += b.x; a.y += b.y; a.z += b.z; a.w += b.w; }

	inline void operator-=(Float2& a, const Float2& b) { a.x -= b.x; a.y -= b.y; }
	inline void operator-=(Float3& a, const Float3& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; }
	inline void operator-=(Float4& a, const Float4& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; a.w -= b.w; }

	inline void operator*=(Float2& a, const Float2& b) { a.x *= b.x; a.y *= b.y; }
	inline void operator*=(Float3& a, const Float3& b) { a.x *= b.x; a.y *= b.y; a.z *= b.z; }
	inline void operator*=(Float4& a, const Float4& b) { a.x *= b.x; a.y *= b.y; a.z *= b.z; a.w *= b.w; }

	inline void operator/=(Float2& a, const Float2& b) { a.x /= b.x; a.y /= b.y; }
	inline void operator/=(Float3& a, const Float3& b) { a.x /= b.x; a.y /= b.y; a.z /= b.z; }
	inline void operator/=(Float4& a, const Float4& b) { a.x /= b.x; a.y /= b.y; a.z /= b.z; a.w /= b.w; }

	// --- Vector specific operations

	inline float dot(const Float2& a, const Float2& b) { return a.x * b.x + a.y * b.y; }
	inline float dot(const Float3& a, const Float3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline float dot(const Float4& a, const Float4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

	inline Float2 normalize(const Float2& vec) { float invLen = rsqrtf(dot(vec, vec)); return invLen * vec; }
	inline Float3 normalize(const Float3& vec) { float invLen = rsqrtf(dot(vec, vec)); return invLen * vec; }
	inline Float4 normalize(const Float4& vec) { float invLen = rsqrtf(dot(vec, vec)); return invLen * vec; }
}
