#pragma once

#include <cstdint>
#include <vector>

#include "hri_math.h"
#include "renderer_internal/render_context.h"
#include "renderer_internal/buffer.h"

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
		/// @brief Instantiate a new Mesh object.
		/// @param ctx Render Context to use.
		/// @param vertices A vector of vertices to draw, may not be empty.
		/// @param indices A vector of indices into the vertex array, may not be empty.
		/// @param bufferFlags Additional create flags to specify for vertex & index buffers.
		Mesh(RenderContext& ctx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, VkBufferUsageFlags bufferFlags = 0);

		/// @brief Destroy this Mesh object.
		virtual ~Mesh() = default;

		// Allow move semantics
		Mesh(Mesh&&) = default;
		Mesh& operator=(Mesh&&) = default;

	public:
		uint32_t vertexCount	= 0;
		uint32_t indexCount		= 0;
		BufferResource vertexBuffer;
		BufferResource indexBuffer;
	};
}
