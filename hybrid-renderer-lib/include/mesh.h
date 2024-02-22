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

	/// @brief The GPU Mesh struct contains host visible vertex & index buffers as well as metadata.
	struct GPUMesh
	{
		uint32_t indexCount				= 0;
		uint32_t vertexCount			= 0;
		BufferResource vertexBuffer		= BufferResource{};
		BufferResource indexBuffer		= BufferResource{};

		/// @brief Initialize a new GPU Mesh.
		/// @param ctx Render Context to use.
		/// @param vertices Vertex array.
		/// @param indices Index array.
		/// @return A new GPU Mesh.
		static GPUMesh init(RenderContext* ctx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

		/// @brief Destroy a GPU Mesh.
		/// @param ctx Render context used to create the GPU Mesh.
		/// @param mesh Mesh to destroy.
		static void destroy(RenderContext* ctx, GPUMesh& mesh);
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

		/// @brief Create a GPU Mesh object from this mesh, allocating device buffers & setting metadata.
		/// @param ctx Render Context to use.
		/// @return A new GPUMesh.
		GPUMesh createGPUMesh(RenderContext* ctx);

	public:
		std::vector<Vertex> vertices	= {};
		std::vector<uint32_t> indices	= {};
	};
}
