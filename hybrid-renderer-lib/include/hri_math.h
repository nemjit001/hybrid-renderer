#pragma once

#include <cmath>
#include <immintrin.h>

#include "platform.h"

#define HRI_PI			3.14159265358979323846264f
#define HRI_INV_PI		0.31830988618379067153777f
#define HRI_2PI			6.28318530717958647692528f
#define HRI_INV_2PI		0.15915494309189533576888f

namespace hri
{
	struct ALIGNAS(4) Float2
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

	struct ALIGNAS(4) Float3
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

	struct ALIGNAS(4) Float4
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

	struct ALIGNAS(4) Float3x3
	{
	public:
		inline Float3x3() {};
		inline Float3x3(float val) {};

	public:
		union {
			struct {
				float m0[3], m1[3], m2[3];
			};

			struct {
				__m128 m04, m14, m24;
			};
		};
	};

	struct ALIGNAS(4) Float4x4
	{
	public:
		inline Float4x4() {};
		inline Float4x4(float val) {};

	public:
		union {
			struct {
				float m0[4], m1[4], m2[4], m3[4];
			};

			struct {
				__m128 m04, m14, m24, m34;
			};
		};
	};

	/// @brief Simple inverse square root function.
	/// @param val Value to calculate inverse sqrt for.
	/// @return The inverse square root of val.
	inline float rsqrtf(float val) { return 1.0f / sqrtf(val); }

	inline float degrees(float radians) { return radians * 180.0f * HRI_INV_PI; /* rad * (180 / pi) */ }

	inline float radians(float degrees) { return degrees * HRI_PI * 0.00555555555555555555555555555556f; /* deg * (pi / 180) */ }

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

	inline Float3 cross(const Float3& a, const Float3& b) { return Float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

	inline float dot(const Float2& a, const Float2& b) { return a.x * b.x + a.y * b.y; }
	inline float dot(const Float3& a, const Float3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline float dot(const Float4& a, const Float4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

	inline Float2 normalize(const Float2& vec) { float invLen = rsqrtf(dot(vec, vec)); return invLen * vec; }
	inline Float3 normalize(const Float3& vec) { float invLen = rsqrtf(dot(vec, vec)); return invLen * vec; }
	inline Float4 normalize(const Float4& vec) { float invLen = rsqrtf(dot(vec, vec)); return invLen * vec; }

	inline float magnitude(const Float2& vec) { return sqrtf(dot(vec, vec)); }
	inline float magnitude(const Float3& vec) { return sqrtf(dot(vec, vec)); }
	inline float magnitude(const Float4& vec) { return sqrtf(dot(vec, vec)); }
}
