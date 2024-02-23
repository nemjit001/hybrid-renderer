#include "mesh.h"

#include <cstdint>
#include <vector>

#include "hri_math.h"
#include "renderer_internal/render_context.h"
#include "renderer_internal/buffer.h"

using namespace hri;

Mesh::Mesh(RenderContext* ctx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	:
	vertexCount(static_cast<uint32_t>(vertices.size())),
	indexCount(static_cast<uint32_t>(indices.size())),
	vertexBuffer(ctx, sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true),
	indexBuffer(ctx, sizeof(uint32_t) * indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true)
{
	vertexBuffer.copyToBuffer(vertices.data(), vertexBuffer.bufferSize);
	indexBuffer.copyToBuffer(indices.data(), indexBuffer.bufferSize);
}
