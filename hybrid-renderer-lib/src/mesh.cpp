#include "mesh.h"

#include <cstdint>
#include <vector>

#include "hri_math.h"
#include "renderer_internal/render_context.h"
#include "renderer_internal/buffer.h"

using namespace hri;

void GPUMesh::init(RenderContext* ctx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) noexcept
{
	m_pCtx = ctx;
	assert(m_pCtx != nullptr);

	size_t indexBufferByteSize = sizeof(uint32_t) * indices.size();
	size_t vertexBufferByteSize = sizeof(Vertex) * vertices.size();

	indexCount = static_cast<uint32_t>(indices.size());
	vertexCount = static_cast<uint32_t>(vertices.size());
	indexBuffer = BufferResource::init(m_pCtx, indexBufferByteSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true);
	vertexBuffer = BufferResource::init(m_pCtx, vertexBufferByteSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true);

	indexBuffer.copyToBuffer(m_pCtx, indices.data(), indexBufferByteSize);
	vertexBuffer.copyToBuffer(m_pCtx, vertices.data(), vertexBufferByteSize);
}

void GPUMesh::destroy() noexcept
{
	BufferResource::destroy(m_pCtx, indexBuffer);
	BufferResource::destroy(m_pCtx, vertexBuffer);
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	:
	vertices(vertices),
	indices(indices)
{
	//
}

GPUMesh Mesh::createGPUMesh(RenderContext* ctx) const
{
	GPUMesh mesh = GPUMesh();
	mesh.init(ctx, vertices, indices);
	
	return mesh;
}
