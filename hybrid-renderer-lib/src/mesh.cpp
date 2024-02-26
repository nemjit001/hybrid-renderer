#include "mesh.h"

#include <cstdint>
#include <vector>

#include "hri_math.h"
#include "renderer_internal/render_context.h"
#include "renderer_internal/buffer.h"
#include "renderer_internal/command_submission.h"

using namespace hri;

Mesh::Mesh(RenderContext& ctx, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	:
	vertexCount(static_cast<uint32_t>(vertices.size())),
	indexCount(static_cast<uint32_t>(indices.size())),
	vertexBuffer(ctx, sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
	indexBuffer(ctx, sizeof(uint32_t) * indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
{
	CommandPool pool = CommandPool(ctx, ctx.queues.transferQueue);

	BufferResource& stagingVertex = BufferResource(ctx, vertexBuffer.bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
	BufferResource& stagingIndex = BufferResource(ctx, indexBuffer.bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);

	stagingVertex.copyToBuffer(vertices.data(), vertexBuffer.bufferSize);
	stagingIndex.copyToBuffer(indices.data(), indexBuffer.bufferSize);

	VkCommandBuffer commandBuffer = pool.createCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VkBufferCopy vertexCopy = VkBufferCopy{};
	vertexCopy.srcOffset = 0;
	vertexCopy.dstOffset = 0;
	vertexCopy.size = vertexBuffer.bufferSize;

	VkBufferCopy indexCopy = VkBufferCopy{};
	indexCopy.srcOffset = 0;
	indexCopy.dstOffset = 0;
	indexCopy.size = indexBuffer.bufferSize;

	vkCmdCopyBuffer(commandBuffer, stagingVertex.buffer, vertexBuffer.buffer, 1, &vertexCopy);
	vkCmdCopyBuffer(commandBuffer, stagingIndex.buffer, indexBuffer.buffer, 1, &indexCopy);

	pool.submitAndWait(commandBuffer);
}
