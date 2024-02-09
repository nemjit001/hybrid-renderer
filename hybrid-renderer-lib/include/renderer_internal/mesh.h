#pragma once

#include <cstdint>
#include <vector>

#include "renderer_internal/hri_math.h"

namespace hri
{
	/// @brief A Vertex is a datapoint containing information needed for rendering.
	struct Vertex
	{
		Float3 position;
		Float3 normal;
		Float3 tangent;
		Float2 textureCoord;
	};

	/// @brief A Mesh represents a renderable object. It uses indexed drawing to reduce vertex array size.
	class Mesh
	{
	public:
		Mesh() = delete;

		/// @brief Instantiate a new Mesh object.
		/// @param vertices A vector of vertices to draw, may not be empty.
		/// @param indices A vector of indices into the vertex array, may not be empty.
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

		/// @brief Destroy this Mesh object.
		virtual ~Mesh() = default;

	public:
		std::vector<Vertex> vertices	= {};
		std::vector<uint32_t> indices	= {};
	};
}
