#include "scene.h"

#include <fstream>
#include <hybrid_renderer.h>
#include <nlohmann/json.hpp>
#include <string>
#include <tiny_obj_loader.h>

#include "demo.h"

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
	nodes(std::move(nodes))
{
	// TODO: build BLASses & TLAS
}

void SceneGraph::update(float deltaTime)
{
	// TODO: rebuild TLAS & BLASses
}

const std::vector<RenderInstance>& SceneGraph::getInstanceData() const
{
	return m_instances;
}

const std::vector<RenderInstance>& SceneGraph::generateInstanceData(const hri::Camera& camera)
{
	m_instances.clear();

	for (auto const& node : nodes)
	{
		SceneNode::SceneId meshLOD = calculateLODLevel(camera, node);
		assert(meshLOD != INVALID_SCENE_ID);

		// TODO: upload mesh data into scene buffers, build TLAS from instance BLASses
		m_instances.push_back(RenderInstance{
			node.transform.modelMatrix(),
			&materials.at(node.material),
			&meshes.at(meshLOD),
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
				meshes.push_back(std::move(hri::Mesh(context.renderContext, vertices, indices)));
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
			MaterialParameters materialParams = MaterialParameters{};
			materialParams.diffuse = hri::Float3(objMaterial.diffuse[0], objMaterial.diffuse[1], objMaterial.diffuse[2]);
			materialParams.specular = hri::Float3(objMaterial.specular[0], objMaterial.specular[1], objMaterial.specular[2]);
			materialParams.transmittance = hri::Float3(objMaterial.transmittance[0], objMaterial.transmittance[1], objMaterial.transmittance[2]);
			materialParams.emission = hri::Float3(objMaterial.emission[0], objMaterial.emission[1], objMaterial.emission[2]);
			materialParams.shininess = objMaterial.shininess;
			materialParams.ior = objMaterial.ior;

			material = Material{
				materialParams,
				hri::Texture(),
				hri::Texture(),
			};
		}

		break;
	}

	return vertices.size() > 0 && indices.size() > 0;
}
