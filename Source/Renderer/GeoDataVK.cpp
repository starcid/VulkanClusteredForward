#include "GeoDataVK.h"
#include "Renderer/VRenderer.h"

const std::vector<Vertex> vertices = {
	{{0.0f, -2.5f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
	{{2.5f, 2.5f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	{{-2.5f, 2.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

	{{0.0f, 0.0f, 0.01f, 1.0f}, {1.0f, 0.0f, 0.0f}},
	{{5.0f, -2.5f, 0.01f, 1.0f}, {0.0f, 1.0f, 0.0f}},
	{{-2.5f, -2.5f, 0.01f, 1.0f}, {0.0f, 0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 3, 5, 4
};

GeoDataVK::GeoDataVK(Renderer* renderer)
	:GeoData(renderer)
{
	
}

GeoDataVK::~GeoDataVK()
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)m_pRenderer;

	for (int i = 0; i < meshlet_buffers.size(); i++)
	{
		vRenderer->CleanBuffer(meshlet_buffers[i], meshlet_buffers_memorys[i]);
	}
	meshlet_buffers.clear();
	meshlet_buffers_memorys.clear();
	for (int i = 0; i < vertex_storage_buffers.size(); i++)
	{
		vRenderer->CleanBuffer(vertex_storage_buffers[i], vertex_storage_buffer_memorys[i]);
	}
	vertex_storage_buffers.clear();
	vertex_storage_buffer_memorys.clear();

	if (vRenderer->IsMeshShadingSupported())
	{
		for (int i = 0; i < desc_sets_data.size(); i++)
		{
			vRenderer->FreeMeshletDescriptorSets(desc_sets_data[i]);
			delete[] desc_sets_data[i];
		}
	}

	for (int i = 0; i < index_buffers.size(); i++)
	{
		vRenderer->CleanBuffer(index_buffers[i], index_buffer_memorys[i]);
	}
	index_buffers.clear();
	index_buffer_memorys.clear();
	for (int i = 0; i < vertex_buffers.size(); i++)
	{
		vRenderer->CleanBuffer(vertex_buffers[i], vertex_buffer_memorys[i]);
	}
	vertex_buffers.clear();
	vertex_buffer_memorys.clear();
}

void GeoDataVK::initTestData()
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)m_pRenderer;

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;
	VkBuffer index_buffer;
	VkDeviceMemory index_buffer_memory;
	vRenderer->CreateVertexBuffer((void*)vertices.data(), sizeof(vertices[0]), vertices.size(), vertex_buffer, vertex_buffer_memory);
	vertex_buffers.push_back(vertex_buffer);
	vertex_buffer_memorys.push_back(vertex_buffer_memory);
	vRenderer->CreateIndexBuffer((void*)indices.data(), sizeof(indices[0]), indices.size(), index_buffer, index_buffer_memory);
	index_buffers.push_back(index_buffer);
	index_buffer_memorys.push_back(index_buffer_memory);
	indices_counts.push_back(static_cast<uint32_t>(indices.size()));
}

void GeoDataVK::initTinyObjData(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials)
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)m_pRenderer;
	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_buffer_memory;

	/// multi vertex buffers
	bool hasWeight = attrib.vertex_weights.size() > 0;
	bool hasWs = attrib.texcoord_ws.size() > 0;
	for (int i = 0; i < shapes.size(); i++)
	{
		tinyobj::shape_t* shape = &shapes[i];
		tinyobj::mesh_t* mesh = &shape->mesh;
		int totalVtxNum = mesh->indices.size();
		assert(mesh->num_face_vertices.size() == totalVtxNum / 3);

		/// sub mesh precalculate
		std::vector<int> subMeshTriIdxs;
		std::vector<int> subMeshMatIds;
		int matId = mesh->material_ids[0];
		for (int j = 0; j < mesh->material_ids.size(); j++)
		{
			if (mesh->material_ids[j] != matId)
			{
				subMeshMatIds.push_back(matId);
				subMeshTriIdxs.push_back(j);
				matId = mesh->material_ids[j];
			}
		}
		subMeshMatIds.push_back(matId);
		subMeshTriIdxs.push_back((int)mesh->material_ids.size());

		/// mesh data
		/*MeshData meshData;
		int totalVtxCount = attrib.vertices.size() / 3;
		Vertex* vertices = new Vertex[totalVtxCount];
		for (int j = 0; j < totalVtxCount; j++)
		{
			memcpy(&vertices[j].pos, &attrib.vertices[j * 3], sizeof(float) * 3);
			vertices[j].pos.w = 1.0f;
			if (hasWeight)
				vertices[j].pos.w = attrib.vertex_weights[j];
			memcpy(&vertices[j].color, &attrib.colors[j * 3], sizeof(float) * 3);
			vertices[j].texcoord.x = attrib.texcoords[j * 2 + 0];
			vertices[j].texcoord.y = 1.0f - attrib.texcoords[j * 2 + 1];
			if (hasWs)
				vertices[j].texcoord.z = attrib.texcoord_ws[j];
			memcpy(&vertices[j].normal, &attrib.normals[j * 3], sizeof(float) * 3);
			if ((j % 3) == 2)
			{
				glm::vec4& v0 = vertices[j - 2].pos;
				glm::vec4& v1 = vertices[j - 1].pos;
				glm::vec4& v2 = vertices[j].pos;

				// Shortcuts for UVs
				glm::vec3& uv0 = vertices[j - 2].texcoord;
				glm::vec3& uv1 = vertices[j - 1].texcoord;
				glm::vec3& uv2 = vertices[j].texcoord;

				// Edges of the triangle : position delta
				glm::vec3 deltaPos1 = v1 - v0;
				glm::vec3 deltaPos2 = v2 - v0;

				// UV delta
				glm::vec2 deltaUV1 = uv1 - uv0;
				glm::vec2 deltaUV2 = uv2 - uv0;

				float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
				glm::vec3 tangent = glm::normalize((deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r);
				glm::vec3 bitangent = glm::normalize((deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r);

				vertices[j - 2].tangent = tangent;
				vertices[j - 1].tangent = tangent;
				vertices[j].tangent = tangent;

				vertices[j - 2].bitangent = bitangent;
				vertices[j - 1].bitangent = bitangent;
				vertices[j].bitangent = bitangent;
			}
		}*/

		/// sub mesh vb
		int startVtxIdx = 0;
		for (int k = 0; k < subMeshMatIds.size(); k++)
		{
			int vtxNum = (subMeshTriIdxs[k] - startVtxIdx) * 3;
			Vertex* vertices = new Vertex[vtxNum];
			for (int j = 0; j < vtxNum; j++)
			{
				int idx = mesh->indices[startVtxIdx * 3 + j].vertex_index;
				vertices[j].pos.x = attrib.vertices[idx * 3 + 0];
				vertices[j].pos.y = attrib.vertices[idx * 3 + 1];
				vertices[j].pos.z = attrib.vertices[idx * 3 + 2];
				vertices[j].pos.w = 1.0f;
				if (hasWeight)
					vertices[j].pos.w = attrib.vertex_weights[idx];
				vertices[j].color.r = attrib.colors[idx * 3 + 0];
				vertices[j].color.g = attrib.colors[idx * 3 + 1];
				vertices[j].color.b = attrib.colors[idx * 3 + 2];

				idx = mesh->indices[startVtxIdx * 3 + j].texcoord_index;
				vertices[j].texcoord.x = attrib.texcoords[idx * 2 + 0];
				vertices[j].texcoord.y = 1.0f - attrib.texcoords[idx * 2 + 1];
				if (hasWs)
				{
					vertices[j].texcoord.z = attrib.texcoord_ws[idx];
				}

				idx = mesh->indices[startVtxIdx * 3 + j].normal_index;
				vertices[j].normal.x = attrib.normals[idx * 3 + 0];
				vertices[j].normal.y = attrib.normals[idx * 3 + 1];
				vertices[j].normal.z = attrib.normals[idx * 3 + 2];

				/// calculate tangent/bitangent
				if ((j % 3) == 2)
				{
					glm::vec4& v0 = vertices[j - 2].pos;
					glm::vec4& v1 = vertices[j - 1].pos;
					glm::vec4& v2 = vertices[j].pos;

					// Shortcuts for UVs
					glm::vec3& uv0 = vertices[j - 2].texcoord;
					glm::vec3& uv1 = vertices[j - 1].texcoord;
					glm::vec3& uv2 = vertices[j].texcoord;

					// Edges of the triangle : position delta
					glm::vec3 deltaPos1 = v1 - v0;
					glm::vec3 deltaPos2 = v2 - v0;

					// UV delta
					glm::vec2 deltaUV1 = uv1 - uv0;
					glm::vec2 deltaUV2 = uv2 - uv0;

					float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
					glm::vec3 tangent = glm::normalize((deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r);
					glm::vec3 bitangent = glm::normalize((deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r);

					vertices[j - 2].tangent = tangent;
					vertices[j - 1].tangent = tangent;
					vertices[j].tangent = tangent;

					vertices[j - 2].bitangent = bitangent;
					vertices[j - 1].bitangent = bitangent;
					vertices[j].bitangent = bitangent;
				}
			}
			for (int j = 0; j < vtxNum; j++)
			{
				glm::vec3& n = vertices[j].normal;
				glm::vec3& t = vertices[j].tangent;
				glm::vec3& b = vertices[j].bitangent;

				// Gram-Schmidt orthogonalize
				t = glm::normalize(t - n * glm::dot(n, t));

				// Calculate handedness
				if (glm::dot(glm::cross(n, t), b) < 0.0f) {
					t = t * -1.0f;
				}
			}
			vRenderer->CreateVertexBuffer((void*)vertices, sizeof(Vertex), vtxNum, vertex_buffer, vertex_buffer_memory);
			vertex_buffers.push_back(vertex_buffer);
			vertex_buffer_memorys.push_back(vertex_buffer_memory);
			indices_counts.push_back(static_cast<uint32_t>(vtxNum));
			/// meshlet
			if (vRenderer->IsMeshShadingSupported())
			{
				GenerateMeshlets((void*)vertices, vtxNum);
			}
			mat_ids.push_back(subMeshMatIds[k]);
			delete[] vertices;

			startVtxIdx = subMeshTriIdxs[k];
		}
	}
}

void GeoDataVK::GenerateMeshlets(void* vtxData, int vtxNum)
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)m_pRenderer;
	Vertex* vertices = (Vertex*)vtxData;

	Vertex_Storage* vertices_storage = new Vertex_Storage[vtxNum];
	for (int i = 0; i < vtxNum; i++)
	{
		///memcpy(vertices_storage + i, vertices + i, sizeof(Vertex));
		vertices_storage[i].pos = vertices[i].pos;
		vertices_storage[i].color = vertices[i].color;
		vertices_storage[i].texcoord = vertices[i].texcoord;
		vertices_storage[i].normal = vertices[i].normal;
		vertices_storage[i].tangent = vertices[i].tangent;
		vertices_storage[i].bitangent = vertices[i].bitangent;
	}

	void* verticesDatas;
	VkBuffer vertex_storage_buffer;
	VkDeviceMemory vertex_storage_buffer_memory;
	vRenderer->CreateLocalStorageBuffer(&verticesDatas, sizeof(Vertex_Storage) * vtxNum, vertex_storage_buffer, vertex_storage_buffer_memory);
	memcpy(verticesDatas, vertices_storage, sizeof(Vertex_Storage) * vtxNum);	// set to gpu
	vRenderer->UnmapBufferMemory(vertex_storage_buffer_memory); // unmap ok
	vertex_storage_buffers.push_back(vertex_storage_buffer);
	vertex_storage_buffer_memorys.push_back(vertex_storage_buffer_memory);

	VkDescriptorBufferInfo vertex_storage_buffer_info;
	vertex_storage_buffer_info.buffer = vertex_storage_buffer;
	vertex_storage_buffer_info.offset = 0;
	vertex_storage_buffer_info.range = sizeof(Vertex_Storage) * vtxNum;
	vertex_storage_buffer_infos.push_back(vertex_storage_buffer_info);

	int triangleNum = vtxNum / 3;
	int max_primitive = MAX_MESH_SHADER_VERTICES / 3;	// no indice buffer, so this is low to 21(less than 126... next optimization)
	int max_vtx = max_primitive * 3;
	int meshLetNum = triangleNum / max_primitive + ((triangleNum % max_primitive) != 0 ? 1 : 0);
	Meshlet* mestLets = new Meshlet[meshLetNum];
	memset(mestLets, 0, sizeof(Meshlet) * meshLetNum);
	for (int i = 0; i < vtxNum; i++)
	{
		int meshLetIdx = i / max_vtx;
		int verticesIdx = i % max_vtx;
		mestLets[meshLetIdx].vertices[verticesIdx] = i;
		mestLets[meshLetIdx].vertex_count++;
	}

	VkBuffer meshlet_buffer;
	VkDeviceMemory meshlet_buffer_memory;
	void* mestLetDatas;
	vRenderer->CreateLocalStorageBuffer(&mestLetDatas, sizeof(Meshlet) * meshLetNum, meshlet_buffer, meshlet_buffer_memory);
	memcpy(mestLetDatas, mestLets, sizeof(Meshlet) * meshLetNum);	// set to gpu
	vRenderer->UnmapBufferMemory(meshlet_buffer_memory);	// unmap ok
	meshlet_nums.push_back(meshLetNum);
	meshlet_buffers.push_back(meshlet_buffer);
	meshlet_buffers_memorys.push_back(meshlet_buffer_memory);

	VkDescriptorBufferInfo meshlet_buffer_info;
	meshlet_buffer_info.buffer = meshlet_buffer;
	meshlet_buffer_info.offset = 0;
	meshlet_buffer_info.range = sizeof(Meshlet) * meshLetNum;
	meshlet_buffer_infos.push_back(meshlet_buffer_info);

	delete[] mestLets;
	delete[] vertices_storage;

	VkDescriptorSet* desc_sets = new VkDescriptorSet[3];
	vRenderer->AllocateMeshletDescriptorSets(desc_sets);
	desc_sets_data.push_back(desc_sets);

	/// upload mesh let here
	vRenderer->UploadMeshlets(&meshlet_buffer_info, &vertex_storage_buffer_info, desc_sets);
}