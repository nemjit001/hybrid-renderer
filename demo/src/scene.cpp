#include "scene.h"

#include <fstream>
#include <hybrid_renderer.h>
#include <nlohmann/json.hpp>
#include <string>
#include <tiny_obj_loader.h>

#include "demo.h"

SceneASManager::SceneASManager(
	raytracing::RayTracingContext& ctx,
	const std::vector<hri::Mesh>& meshes
)
	:
	m_ctx(ctx),
	m_asBuilder(ctx)
{
	createBLASList(meshes);
}

bool SceneASManager::shouldReallocTLAS(const raytracing::AccelerationStructure& tlas, const std::vector<RenderInstance>& instances) const
{
	size_t instanceCount = instances.size();
	hri::BufferResource tlasInstanceBuffer = generateTLASInstances(instances);

	raytracing::ASBuilder::ASInput tlasInput = m_asBuilder.instancesToGeometry(
		tlasInstanceBuffer,
		instanceCount,
		false,
		0,
		VK_GEOMETRY_OPAQUE_BIT_KHR
	);

	raytracing::ASBuilder::ASSizeInfo tlasSizeInfo = raytracing::ASBuilder::ASSizeInfo{};
	raytracing::ASBuilder::ASBuildInfo tlasBuildInfo = m_asBuilder.generateASBuildInfo(
		tlasInput,
		tlasSizeInfo,
		VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
	);

	return tlas.buffer.bufferSize < tlasSizeInfo.totalAccelerationStructureSize;
}

raytracing::AccelerationStructure SceneASManager::createTLAS(const std::vector<RenderInstance>& instances)
{
	size_t instanceCount = instances.size();
	hri::BufferResource tlasInstanceBuffer = generateTLASInstances(instances);

	raytracing::ASBuilder::ASInput tlasInput = m_asBuilder.instancesToGeometry(
		tlasInstanceBuffer,
		instanceCount,
		false,
		0,
		VK_GEOMETRY_OPAQUE_BIT_KHR
	);

	raytracing::ASBuilder::ASSizeInfo tlasSizeInfo = raytracing::ASBuilder::ASSizeInfo{};
	raytracing::ASBuilder::ASBuildInfo tlasBuildInfo = m_asBuilder.generateASBuildInfo(
		tlasInput,
		tlasSizeInfo,
		VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
	);

	return raytracing::AccelerationStructure(
		m_ctx,
		VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		tlasSizeInfo.totalAccelerationStructureSize
	);
}

void SceneASManager::cmdBuildTLAS(
	VkCommandBuffer commandBuffer,
	const std::vector<RenderInstance>& instances,
	raytracing::AccelerationStructure& tlas
) const
{
	size_t instanceCount = instances.size();
	hri::BufferResource tlasInstanceBuffer = generateTLASInstances(instances);

	raytracing::ASBuilder::ASInput tlasInput = m_asBuilder.instancesToGeometry(
		tlasInstanceBuffer,
		instanceCount,
		false,
		0,
		VK_GEOMETRY_OPAQUE_BIT_KHR
	);

	raytracing::ASBuilder::ASSizeInfo tlasSizeInfo = raytracing::ASBuilder::ASSizeInfo{};	// XXX: reset size, otherwise size is incremented each call
	raytracing::ASBuilder::ASBuildInfo tlasBuildInfo = m_asBuilder.generateASBuildInfo(
		tlasInput,
		tlasSizeInfo,
		VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
	);

	hri::BufferResource scratchBuffer = hri::BufferResource(
		m_ctx.renderContext,
		tlasSizeInfo.maxBuildScratchBufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
	);

	VkAccelerationStructureKHR tlasHandle = tlas.accelerationStructure;
	m_asBuilder.cmdBuildAccelerationStructure(
		commandBuffer,
		tlasBuildInfo,
		tlasHandle,
		raytracing::getDeviceAddress(m_ctx, scratchBuffer)
	);
}

void SceneASManager::cmdBuildBLASses(VkCommandBuffer commandBuffer, const std::vector<hri::Mesh>& meshes) const
{
	assert(meshes.size() == m_blasList.size());

	raytracing::ASBuilder::ASSizeInfo blasSizeInfo = raytracing::ASBuilder::ASSizeInfo{};
	std::vector<raytracing::ASBuilder::ASInput> blasInputs = generateBLASInputs(meshes);
	std::vector<raytracing::ASBuilder::ASBuildInfo> blasBuildInfos = m_asBuilder.generateASBuildInfo(
		blasInputs,
		blasSizeInfo,
		VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
	);

	hri::BufferResource scratchBuffer = hri::BufferResource(
		m_ctx.renderContext,
		blasSizeInfo.maxBuildScratchBufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
	);

	std::vector<VkAccelerationStructureKHR> asHandles = {}; asHandles.reserve(m_blasList.size());
	for (auto const& blas : m_blasList) asHandles.push_back(blas.accelerationStructure);

	m_asBuilder.cmdBuildAccelerationStructures(
		commandBuffer,
		blasBuildInfos,
		asHandles,
		raytracing::getDeviceAddress(m_ctx, scratchBuffer)
	);
}

hri::BufferResource SceneASManager::generateTLASInstances(const std::vector<RenderInstance>& instances) const
{
	size_t instanceCount = instances.size();
	hri::BufferResource tlasInstanceBuffer = hri::BufferResource(
		m_ctx.renderContext,
		sizeof(VkAccelerationStructureInstanceKHR) * instanceCount,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		true
	);

	std::vector<VkAccelerationStructureInstanceKHR> tlasInstances = {}; tlasInstances.reserve(instances.size());
	for (auto const& renderInstance : instances)
	{
		auto const& blas = m_blasList[renderInstance.instanceId];
		VkAccelerationStructureDeviceAddressInfoKHR addrInfo = VkAccelerationStructureDeviceAddressInfoKHR{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		addrInfo.accelerationStructure = blas.accelerationStructure;

		VkDeviceAddress blasAddress = m_ctx.accelStructDispatch.vkGetAccelerationStructureDeviceAddress(
			m_ctx.renderContext.device,
			&addrInfo
		);

		VkAccelerationStructureInstanceKHR tlasInstance = VkAccelerationStructureInstanceKHR{};
		tlasInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		tlasInstance.transform = raytracing::toTransformMatrix(renderInstance.modelMatrix);
		tlasInstance.instanceCustomIndex = renderInstance.instanceId;
		tlasInstance.accelerationStructureReference = blasAddress;
		tlasInstance.mask = 0xFF;
		tlasInstance.instanceShaderBindingTableRecordOffset = 0;

		tlasInstances.push_back(tlasInstance);
	}

	tlasInstanceBuffer.copyToBuffer(tlasInstances.data(), tlasInstanceBuffer.bufferSize);
	return tlasInstanceBuffer;
}

std::vector<raytracing::ASBuilder::ASInput> SceneASManager::generateBLASInputs(const std::vector<hri::Mesh>& meshes) const
{
	std::vector<raytracing::ASBuilder::ASInput> blasInputs = {}; blasInputs.reserve(meshes.size());

	for (auto const& mesh : meshes)
	{
		raytracing::ASBuilder::ASInput input = m_asBuilder.objectToGeometry(
			mesh,
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
			VK_GEOMETRY_OPAQUE_BIT_KHR
		);

		blasInputs.push_back(input);
	}

	return blasInputs;
}

void SceneASManager::createBLASList(const std::vector<hri::Mesh>& meshes)
{
	raytracing::ASBuilder::ASSizeInfo blasSizeInfo = raytracing::ASBuilder::ASSizeInfo{};
	std::vector<raytracing::ASBuilder::ASInput> blasInputs = generateBLASInputs(meshes);
	std::vector<raytracing::ASBuilder::ASBuildInfo> blasBuildInfos = m_asBuilder.generateASBuildInfo(
		blasInputs,
		blasSizeInfo,
		VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
	);

	m_blasList.clear();
	m_blasList.reserve(blasBuildInfos.size());
	for (auto const& buildInfo : blasBuildInfos)
	{
		raytracing::AccelerationStructure blas = raytracing::AccelerationStructure(
			m_ctx,
			VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			buildInfo.buildSizes.accelerationStructureSize
		);

		m_blasList.push_back(std::move(blas));
	}
}

SceneGraph::SceneGraph(
	raytracing::RayTracingContext& ctx,
	std::vector<Material>&& materials,
	std::vector<hri::Mesh>&& meshes,
	std::vector<SceneNode>&& nodes
)
	:
	m_ctx(ctx),
	m_asBuilder(ctx),
	materials(std::move(materials)),
	meshes(std::move(meshes)),
	nodes(std::move(nodes)),
	accelStructManager(ctx, this->meshes),
	buffers{
		hri::BufferResource(
			ctx.renderContext,
			sizeof(RenderInstance)* this->meshes.size(),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		),
		hri::BufferResource(
			ctx.renderContext,
			sizeof(Material) * this->materials.size(),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
		),
	}
{
	hri::CommandPool stagingPool = hri::CommandPool(
		m_ctx.renderContext,
		m_ctx.renderContext.queues.transferQueue,
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
	);

	// Set up render instance data for all meshes
	m_instanceData.resize(this->meshes.size());
	for (auto const& node : this->nodes)
	{
		for (size_t lodIdx = 0; lodIdx < MAX_LOD_LEVELS; lodIdx++)
		{
			size_t meshLOD = node.meshLODs[lodIdx];
			if (meshLOD == INVALID_SCENE_ID)
				continue;

			const hri::Mesh& mesh = this->meshes[meshLOD];
			RenderInstanceData& instanceData = m_instanceData[meshLOD];

			instanceData.materialIdx = static_cast<uint32_t>(node.material);
			instanceData.vertexBufferAddress = raytracing::getDeviceAddress(m_ctx, mesh.vertexBuffer);
			instanceData.indexBufferAddress = raytracing::getDeviceAddress(m_ctx, mesh.indexBuffer);
		}
	}

	// Fill staging buffers
	hri::BufferResource instanceStaging = hri::BufferResource(ctx.renderContext, buffers.instanceDataSSBO.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
	hri::BufferResource materialStaging = hri::BufferResource(ctx.renderContext, buffers.materialSSBO.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);

	instanceStaging.copyToBuffer(this->m_instanceData.data(), buffers.instanceDataSSBO.bufferSize);
	materialStaging.copyToBuffer(this->materials.data(), buffers.materialSSBO.bufferSize);

	// Transfer resources
	VkCommandBuffer transferBuffer = stagingPool.createCommandBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VkBufferCopy instanceSSBOCopy = VkBufferCopy{ 0, 0, buffers.instanceDataSSBO.bufferSize };
	VkBufferCopy matSSBOCopy = VkBufferCopy{ 0, 0, buffers.materialSSBO.bufferSize };

	vkCmdCopyBuffer(transferBuffer, instanceStaging.buffer, buffers.instanceDataSSBO.buffer, 1, &instanceSSBOCopy);
	vkCmdCopyBuffer(transferBuffer, materialStaging.buffer, buffers.materialSSBO.buffer, 1, &matSSBOCopy);

	stagingPool.submitAndWait(transferBuffer);
}

void SceneGraph::update(float deltaTime)
{
	// TODO: rebuild TLAS & BLASses
}

const std::vector<RenderInstance>& SceneGraph::getRenderInstanceList() const
{
	return m_instances;
}

const std::vector<RenderInstance>& SceneGraph::generateRenderInstanceList(const hri::Camera& camera)
{
	m_instances.clear();

	for (auto const& node : nodes)
	{
		SceneNode::SceneId meshLOD = calculateLODLevel(camera, node);
		assert(meshLOD != INVALID_SCENE_ID);

		// TODO: upload mesh data into scene buffers, build TLAS from instance BLASses
		m_instances.push_back(RenderInstance{
			node.transform.modelMatrix(),
			static_cast<uint32_t>(meshLOD),
		});
	}

	return m_instances;
}

SceneNode::SceneId SceneGraph::calculateLODLevel(const hri::Camera& camera, const SceneNode& node)
{
	hri::Float3 camToNode = node.transform.position - camera.position;
	const float nodeDist = hri::magnitude(camToNode);
	const float LODRange = parameters.farPoint - parameters.nearPoint;
	const float LODSegmentSize = LODRange / static_cast<float>(MAX_LOD_LEVELS);

	// XXX: Check -> is Linear LOD sufficient, or should better algorithm be used?
	SceneNode::SceneId LODIndex = 0;
	for (size_t segmentIdx = 0; segmentIdx < MAX_LOD_LEVELS; segmentIdx++)
	{
		float fIdx = static_cast<float>(segmentIdx);
		float minSegmentDist = fIdx * LODSegmentSize;

		if (nodeDist >= minSegmentDist && node.meshLODs[segmentIdx] != INVALID_SCENE_ID)
			LODIndex = segmentIdx;
	}

	return node.meshLODs[LODIndex];
}

SceneGraph SceneLoader::load(raytracing::RayTracingContext& context, const std::string& path)
{
	std::ifstream sceneFile = std::ifstream(path);
	nlohmann::json sceneJSON = nlohmann::json::parse(sceneFile);

	std::vector<Material> materials = {};
	std::vector<hri::Mesh> meshes = {};
	std::vector<SceneNode> nodes = {}; nodes.reserve(sceneJSON["nodes"].size());

	for (auto const& node : sceneJSON["nodes"])
	{
		const std::string& meshFile = node["mesh_file"];
		auto const& transform = node["transform"];

		SceneNode newNode = SceneNode{};
		newNode.name = node["name"];
		newNode.transform = SceneTransform{};
		newNode.material = materials.size();

		// Set node transform
		newNode.transform.position = hri::Float3(transform["position"][0], transform["position"][1], transform["position"][2]);
		newNode.transform.rotation = hri::Float3(transform["rotation"][0], transform["rotation"][1], transform["rotation"][2]);
		newNode.transform.scale = hri::Float3(transform["scale"][0], transform["scale"][1], transform["scale"][2]);

		// Load meshes
		Material newMaterial = Material{};
		uint32_t lodIndex = 0;
		for (auto const& lodLevel : node["lod_levels"])
		{
			bool loadMaterial = lodIndex == 0;	// Only load material of first LOD, assume materials are the same.
			std::vector<hri::Vertex> vertices = {};
			std::vector<uint32_t> indices = {};

			if (loadOBJMesh(meshFile, lodLevel, newMaterial, vertices, indices, loadMaterial))
			{
				newNode.meshLODs[lodIndex] = meshes.size();
				meshes.push_back(std::move(hri::Mesh(
					context.renderContext,
					vertices,
					indices,
					VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
					| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
				)));
			}

			if (lodIndex >= MAX_LOD_LEVELS)
				break;

			lodIndex++;
		}

		materials.push_back(newMaterial);
		nodes.push_back(newNode);
	}

	return SceneGraph(
		context,
		std::move(materials),
		std::move(meshes),
		std::move(nodes)
	);
}

bool SceneLoader::loadOBJMesh(
	const std::string& path,
	const std::string& name,
	Material& material,
	std::vector<hri::Vertex>& vertices,
	std::vector<uint32_t>& indices,
	bool loadMaterial
)
{
	// Only real configuration needed is the triangulation on load
	tinyobj::ObjReaderConfig readerConfig = tinyobj::ObjReaderConfig{};
	readerConfig.triangulate = true;
	readerConfig.triangulation_method = "earcut";

	// Parse file & handle errors / report warnings
	tinyobj::ObjReader reader;
	if (false == reader.ParseFromFile(path, readerConfig))
	{
		if (reader.Error().size() > 0)
		{
			fprintf(stderr, "Loader Error: %s", reader.Error().c_str());
		}

		FATAL_ERROR("Failed to parse OBJ file");
	}

	if (reader.Warning().size() > 0)
	{
		printf("Loader Warning: %s", reader.Warning().c_str());
	}

	// Obj scene file attributes
	const tinyobj::attrib_t& objAttributes = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& objShapes = reader.GetShapes();
	const std::vector<tinyobj::material_t>& objMaterials = reader.GetMaterials();

	vertices.clear();
	indices.clear();

	// Load shapes into hri compatible format
	for (auto const& shape : objShapes)
	{
		if (shape.name != name)
			continue;

		// Retrieve indexed vertices for Shape
		vertices.reserve(shape.mesh.indices.size());
		indices.reserve(shape.mesh.indices.size());
		for (auto const& index : shape.mesh.indices)
		{
			size_t vertexIndex = index.vertex_index * 3;
			size_t normalIndex = index.normal_index * 3;
			size_t texCoordIndex = index.texcoord_index * 2;

			hri::Vertex newVertex = hri::Vertex{
				hri::Float3(
					objAttributes.vertices[vertexIndex + 0],
					objAttributes.vertices[vertexIndex + 1],
					objAttributes.vertices[vertexIndex + 2]
				),
				hri::Float3(
					objAttributes.normals[normalIndex + 0],
					objAttributes.normals[normalIndex + 1],
					objAttributes.normals[normalIndex + 2]
				),
				hri::Float3(0.0f),	// The tangent vector is initialy (0, 0, 0), because it can only be calculated AFTER triangles are loaded
				hri::Float2(
					objAttributes.texcoords[texCoordIndex + 0],
					objAttributes.texcoords[texCoordIndex + 1]
				),
			};

			vertices.push_back(newVertex);
			indices.push_back(static_cast<uint32_t>(indices.size()));	// Works because mesh is already triangulated on load
		}

		// Calculate tangent space vectors for Shape
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			hri::Vertex& v0 = vertices[indices[i + 0]];
			hri::Vertex& v1 = vertices[indices[i + 1]];
			hri::Vertex& v2 = vertices[indices[i + 2]];

			const hri::Float3 e1 = v1.position - v0.position, e2 = v2.position - v0.position;
			const hri::Float2 dUV1 = v1.textureCoord - v0.textureCoord, dUV2 = v2.textureCoord - v0.textureCoord;

			const float f = 1.0f / (dUV1.x * dUV2.y - dUV2.x * dUV1.y);
			const hri::Float3 tangent = normalize(f * (dUV2.y * e1 - dUV1.y * e2));

			// Ensure tangent is 90 degrees w/ normal for each vertex
			v0.tangent = normalize(tangent - dot(v0.normal, tangent) * v0.normal);
			v1.tangent = normalize(tangent - dot(v1.normal, tangent) * v1.normal);
			v2.tangent = normalize(tangent - dot(v2.normal, tangent) * v2.normal);
		}

		if (loadMaterial)
		{
			// Set material params & load textures
			tinyobj::material_t objMaterial = objMaterials[shape.mesh.material_ids[0]];
			material = Material{};

			material.diffuse = hri::Float3(objMaterial.diffuse[0], objMaterial.diffuse[1], objMaterial.diffuse[2]);
			material.specular = hri::Float3(objMaterial.specular[0], objMaterial.specular[1], objMaterial.specular[2]);
			material.transmittance = hri::Float3(objMaterial.transmittance[0], objMaterial.transmittance[1], objMaterial.transmittance[2]);
			material.emission = hri::Float3(objMaterial.emission[0], objMaterial.emission[1], objMaterial.emission[2]);
			material.shininess = objMaterial.shininess;
			material.ior = objMaterial.ior;
		}

		break;
	}

	return vertices.size() > 0 && indices.size() > 0;
}
