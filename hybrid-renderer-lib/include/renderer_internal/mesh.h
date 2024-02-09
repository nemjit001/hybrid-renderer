#pragma once

#include <cstdint>
#include <vector>

namespace hri
{
	/// @brief A Vertex is a datapoint containing information needed for rendering.
	struct Vertex
	{
		//
	};

	/// @brief A Mesh represents a renderable object. It uses indexed drawing to reduce vertex array size.
	class Mesh
	{
	public:
		/// @brief Instantiate a new Mesh object.
		/// @param vertices A vector of vertices to draw, may not be empty.
		/// @param indices A vector of indices into the vertex array, may not be empty.
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

	public:
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};
}
