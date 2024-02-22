#include "mesh.h"

#include <cstdint>
#include <vector>

#include "hri_math.h"
#include "renderer_internal/render_context.h"
#include "renderer_internal/buffer.h"

using namespace hri;

GPUMesh GPUMesh::init(RenderContext* ctx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	assert(ctx != nullptr);

	size_t indexBufferByteSize = sizeof(uint32_t) * indices.size();
	size_t vertexBufferByteSize = sizeof(Vertex) * vertices.size();

	GPUMesh mesh = GPUMesh{};
	mesh.indexCount = indices.size();
	mesh.vertexCount = vertices.size();
	mesh.indexBuffer = BufferResource::init(ctx, indexBufferByteSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true);
	mesh.vertexBuffer = BufferResource::init(ctx, vertexBufferByteSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true);

	mesh.indexBuffer.copyToBuffer(ctx, indices.data(), indexBufferByteSize);
	mesh.vertexBuffer.copyToBuffer(ctx, vertices.data(), vertexBufferByteSize);

	return mesh;
}

void GPUMesh::destroy(RenderContext* ctx, GPUMesh& mesh)
{
	BufferResource::destroy(ctx, mesh.indexBuffer);
	BufferResource::destroy(ctx, mesh.vertexBuffer);

	memset(&mesh, 0, sizeof(GPUMesh));
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	:
	vertices(vertices),
	indices(indices)
{
	//
}

GPUMesh Mesh::createGPUMesh(RenderContext* ctx)
{
	return GPUMesh::init(ctx, vertices, indices);
}
