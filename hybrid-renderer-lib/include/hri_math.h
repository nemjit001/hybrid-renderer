#pragma once

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "platform.h"

#define HRI_PI			3.14159265358979323846264f
#define HRI_INV_PI		0.31830988618379067153777f
#define HRI_2PI			6.28318530717958647692528f
#define HRI_INV_2PI		0.15915494309189533576888f

using namespace glm;

namespace hri
{
	struct HRI_ALIGNAS(4) Float2
	{
	public:
		inline Float2() : x(0.0f), y(0.0f) {};
		inline Float2(float val) : x(val), y(val) {};
		inline Float2(float _x, float _y) : x(_x), y(_y) {};

		inline operator glm::vec2() const { return glm::vec2(x, y); }

	public:
		union
		{
			float xy[2];
			struct { float x, y; };
			struct { float u, v; };
		};
	};

	struct HRI_ALIGNAS(4) Float3
	{
	public:
		inline Float3() : x(0.0f), y(0.0f), z(0.0f) {};
		inline Float3(float val) : x(val), y(val), z(val) {};
		inline Float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {};

		inline operator glm::vec3() const { return glm::vec3(x, y, z); }

	public:
		union
		{
			float xyz[3];
			struct { float x, y, z; };
			struct { float r, g, b; };
		};
	};

	struct HRI_ALIGNAS(4) Float4
	{
	public:
		inline Float4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {};
		inline Float4(float val) : x(val), y(val), z(val), w(val) {};
		inline Float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {};

		inline operator glm::vec4() const { return glm::vec4(x, y, z, w); }

	public:
		union
		{
			float xyzw[4];
			struct { float x, y, z, w; };
			struct { float r, g, b, a; };
		};
	};

	typedef mat3 Float3x3;
	typedef mat4 Float4x4;

	/// @brief Simple inverse square root function.
	/// @param val Value to calculate inverse sqrt for.
	/// @return The inverse square root of val.
	inline float rsqrtf(float val) { return 1.0f / sqrtf(val); }

	inline float degrees(float radians) { return radians * 180.0f * HRI_INV_PI; /* rad * (180 / pi) */ }
	inline float radians(float degrees) { return degrees * HRI_PI * 0.00555555555555555555555555555556f; /* deg * (pi / 180) */ }

	inline float floor(float val) { return std::floor(val); }
	inline float ceil(float val) { return std::ceil(val); }
	inline float clamp(float val, float min, float max) { return (val < min) ? min : (val > max) ? max : val; }

	template<typename _Ty>
	inline void swap(_Ty& a, _Ty& b) { _Ty t = a; a = b; b = t; }

	template<typename _Ty>
	inline _Ty max(const _Ty& a, const _Ty& b) { return a > b ? a : b; }

	template<typename _Ty>
	inline _Ty min(const _Ty& a, const _Ty& b) { return a < b ? a : b; }

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
