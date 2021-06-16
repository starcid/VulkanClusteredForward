#include "GeoDataVK.h"
#include "Renderer/VRenderer.h"

GeoDataVK::GeoDataVK(Renderer* renderer)
	:GeoData(renderer)
{
	
}

GeoDataVK::~GeoDataVK()
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)m_pRenderer;

	for (int i = 0; i < meshDatas.size(); i++)
	{
		MeshData* meshData = &meshDatas[i];

		vRenderer->CleanBuffer(meshData->vb, meshData->vbm);
		vRenderer->CleanBuffer(meshData->vsb, meshData->vsbm);
		
		for (int j = 0; j < meshData->subMeshes.size(); j++)
		{
			SubMeshData* subMeshData = &meshData->subMeshes[j];

			vRenderer->CleanBuffer(subMeshData->ib, subMeshData->ibm);

			if (vRenderer->IsMeshShadingSupported())
			{
				vRenderer->FreeMeshletDescriptorSets(subMeshData->dsets);
				delete[] subMeshData->dsets;

				vRenderer->CleanBuffer(subMeshData->mb, subMeshData->mbm);
				vRenderer->CleanBuffer(subMeshData->pib, subMeshData->pibm);
				vRenderer->CleanBuffer(subMeshData->vib, subMeshData->vibm);
			}
		}
	}
}

void GeoDataVK::initTestData()
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)m_pRenderer;

	MeshData meshData;

	vRenderer->CreateVertexBuffer((void*)vertices.data(), sizeof(vertices[0]), vertices.size(), meshData.vb, meshData.vbm);
	meshData.vertexs = vertices;
	
	SubMeshData subMeshData;
	vRenderer->CreateIndexBuffer((void*)indices.data(), sizeof(indices[0]), indices.size(), subMeshData.ib, subMeshData.ibm);
	subMeshData.indices = indices;

	meshData.subMeshes.push_back(subMeshData);

	meshDatas.push_back(meshData);
}

int32_t GeoDataVK::CalculateHash(int idx1, int idx2, int idx3)
{
	return ( idx1 * 31 + idx2 ) * 31 + idx3;
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
		assert(mesh->num_face_vertices.size() == mesh->indices.size() / 3);

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
		MeshData meshData;
		std::vector<Vertex> &vertexs = meshData.vertexs;
		std::map<int, int> indexMap;

		///  vertex / indices buffer generate
		int startVtxIdx = 0;
		for (int k = 0; k < subMeshMatIds.size(); k++)
		{
			/// sub mesh data
			SubMeshData subMeshData;

			std::vector<int>& indices = subMeshData.indices;
			int indicesNum = (subMeshTriIdxs[k] - startVtxIdx) * 3;

			for (int j = 0; j < indicesNum; j++)
			{
				int hashIdx[3];
				Vertex vertex;
				int idx = mesh->indices[startVtxIdx * 3 + j].vertex_index;
				vertex.pos.x = attrib.vertices[idx * 3 + 0];
				vertex.pos.y = attrib.vertices[idx * 3 + 1];
				vertex.pos.z = attrib.vertices[idx * 3 + 2];
				vertex.pos.w = 1.0f;
				if (hasWeight)
					vertex.pos.w = attrib.vertex_weights[idx];
				vertex.color.x = attrib.colors[idx * 3 + 0];
				vertex.color.y = attrib.colors[idx * 3 + 1];
				vertex.color.z = attrib.colors[idx * 3 + 2];
				vertex.color.w = 1.0f;
				hashIdx[0] = idx;

				idx = mesh->indices[startVtxIdx * 3 + j].texcoord_index;
				vertex.texcoord.x = attrib.texcoords[idx * 2 + 0];
				vertex.texcoord.y = 1.0f - attrib.texcoords[idx * 2 + 1];
				if (hasWs)
					vertex.texcoord.z = attrib.texcoord_ws[idx];
				hashIdx[1] = idx;

				idx = mesh->indices[startVtxIdx * 3 + j].normal_index;
				vertex.normal.x = attrib.normals[idx * 3 + 0];
				vertex.normal.y = attrib.normals[idx * 3 + 1];
				vertex.normal.z = attrib.normals[idx * 3 + 2];
				vertex.normal.w = 1.0f;
				hashIdx[2] = idx;

				int hash = CalculateHash(hashIdx[0], hashIdx[1], hashIdx[2]);
				std::map<int, int>::iterator iter = indexMap.find(hash);
				if (iter != indexMap.end())
				{	/// found
					indices.push_back(iter->second);
				}
				else
				{
					indices.push_back(vertexs.size());
					indexMap.insert(std::make_pair(hash, vertexs.size()));
					vertexs.push_back(vertex);
				}
			}
			startVtxIdx = subMeshTriIdxs[k];
			subMeshData.mid = subMeshMatIds[k];
			meshData.subMeshes.push_back(subMeshData);
		}

		/// tangent fix
		std::vector<int>* checkedTangentVtxIdx = new std::vector<int>[vertexs.size()];
		for (int k = 0; k < meshData.subMeshes.size(); k++)
		{
			/// sub mesh data
			SubMeshData& subMeshData = meshData.subMeshes[k];

			std::vector<int>& indices = subMeshData.indices;
			int indicesNum = indices.size();
			glm::vec3* tangents = new glm::vec3[indicesNum];
			glm::vec3* bitangents = new glm::vec3[indicesNum];
			glm::vec3* normals = new glm::vec3[indicesNum];
			glm::vec3 positions[3];
			glm::vec2 texcoords[3];
			for (int j = 0; j < indicesNum; j++)
			{
				int idx = indices[j];
				positions[j % 3].x = vertexs[idx].pos.x;
				positions[j % 3].y = vertexs[idx].pos.y;
				positions[j % 3].z = vertexs[idx].pos.z;

				texcoords[j % 3].x = vertexs[idx].texcoord.x;
				texcoords[j % 3].y = vertexs[idx].texcoord.y;
				
				normals[j].x = vertexs[idx].normal.x;
				normals[j].y = vertexs[idx].normal.y;
				normals[j].z = vertexs[idx].normal.z;

				/// calculate tangent/bitangent	(these datas should be generated by output plugins)
				if ((j % 3) == 2)
				{
					glm::vec3& v0 = positions[2];
					glm::vec3& v1 = positions[1];
					glm::vec3& v2 = positions[0];

					// Shortcuts for UVs
					glm::vec2& uv0 = texcoords[2];
					glm::vec2& uv1 = texcoords[1];
					glm::vec2& uv2 = texcoords[0];

					// Edges of the triangle : position delta
					glm::vec3 deltaPos1 = v1 - v0;
					glm::vec3 deltaPos2 = v2 - v0;

					// UV delta
					glm::vec2 deltaUV1 = uv1 - uv0;
					glm::vec2 deltaUV2 = uv2 - uv0;

					float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
					glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
					glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

					tangents[j - 2] = tangent;
					tangents[j - 1] = tangent;
					tangents[j] = tangent;

					bitangents[j - 2] = bitangent;
					bitangents[j - 1] = bitangent;
					bitangents[j] = bitangent;
				}
			}
			for (int j = 0; j < indicesNum; j++)
			{
				glm::vec3& n = normals[j];
				glm::vec3& t = tangents[j];
				glm::vec3& b = bitangents[j];

				// Gram-Schmidt orthogonalize
				t = glm::normalize(t - n * glm::dot(n, t));

				// Calculate handedness
				if (glm::dot(glm::cross(n, t), b) < 0.0f) {
					t = t * -1.0f;
				}
			}
			delete[] normals;
			delete[] bitangents;

			/// re-organize tangents
			for (int j = 0; j < indicesNum; j++)
			{
				int vtxIdx = indices[j];
				if (checkedTangentVtxIdx[vtxIdx].size() == 0)
				{
					checkedTangentVtxIdx[vtxIdx].push_back(vtxIdx);
					memcpy(&vertexs[vtxIdx].tangent, &tangents[j], sizeof(glm::vec3));
				}
				else
				{
					for (int k = 0; k < checkedTangentVtxIdx[vtxIdx].size(); k++)
					{
						int checkedVtxIdx = checkedTangentVtxIdx[vtxIdx][k];
						glm::vec4& t = vertexs[checkedVtxIdx].tangent;
						if (t.x == tangents[j].x && t.y == tangents[j].y && t.z == tangents[j].z)
						{
							indices[j] = checkedVtxIdx;
							break;
						}
					}
					if (k == checkedTangentVtxIdx[vtxIdx].size())
					{
						/// not equal any existed vertex tangent
						indices[j] = vertexs.size();
						checkedTangentVtxIdx[vtxIdx].push_back(indices[j]);

						/// copy a existed vertex and give a new tangent
						Vertex vertex;
						memcpy(&vertex, &vertexs[vtxIdx], sizeof(Vertex));
						memcpy(&vertex.tangent, &tangents[j], sizeof(glm::vec3));
						vertexs.push_back(vertex);
					}
				}
			}
			delete[] tangents;

			///  create indice buffer
			vRenderer->CreateIndexBuffer((void*)indices.data(), sizeof(int), indices.size(), subMeshData.ib, subMeshData.ibm);
		}
		delete[] checkedTangentVtxIdx;

		/// create vertex buffer
		vRenderer->CreateVertexBuffer((void*)vertexs.data(), sizeof(Vertex), vertexs.size(), meshData.vb, meshData.vbm);

		/// meshlet
		if (vRenderer->IsMeshShadingSupported())
		{
			GenerateMeshlets(&meshData);
		}

		meshDatas.push_back(meshData);
	}
}

void GeoDataVK::GenerateMeshlets(MeshData* meshData)
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)m_pRenderer;

	std::vector<Vertex>& vertexs = meshData->vertexs;

	vRenderer->CreateLocalStorageBufferWithData((void*)vertexs.data(), sizeof(Vertex) * vertexs.size(), meshData->vsb, meshData->vsbm, meshData->vsbi);

	uint32_t* vertexCheckArray = new uint32_t[vertexs.size()];

	int subMeshCount = meshData->subMeshes.size();
	for (int i = 0; i < subMeshCount; i++)
	{
		SubMeshData* subMeshData = &meshData->subMeshes[i];
		std::vector<uint32_t> vertexIndices;
		std::vector<uint32_t> primitiveIndices;
		std::vector<Meshlet> meshlets;

		int max_primitive = MAX_MESH_SHADER_PRIMITIVE;
		int max_vtx = MAX_MESH_SHADER_VERTICES;
		int triangleNum = subMeshData->indices.size() / 3;

		memset(vertexCheckArray, 0, vertexs.size() * sizeof(uint32_t));

		bool isFull = false;
		Meshlet meshlet;
		meshlet.primBegin = 0;
		meshlet.vertexBegin = 0;
		meshlet.primCount = 0;
		meshlet.vertexCount = 0;

		for (int j = 0; j < triangleNum; j++)
		{
			/// vertex full check
			int offset = 0;
			for (int k = 0; k < 3; k++)
			{
				int indice = subMeshData->indices[j * 3 + k];
				if (vertexCheckArray[indice] == 0)
				{
					if (meshlet.vertexCount + offset >= max_vtx)
					{
						isFull = true;
						break;
					}
					offset++;
				}
			}
			if (meshlet.primCount / 3 >= max_primitive)
			{
				isFull = true;
			}
			if (isFull)
			{
				j--;
				isFull = false;
				meshlets.push_back(meshlet);
				memset(vertexCheckArray, 0, vertexs.size() * sizeof(uint32_t));
				meshlet.primBegin = meshlet.primBegin + meshlet.primCount;
				meshlet.vertexBegin = meshlet.vertexBegin + meshlet.vertexCount;
				meshlet.primCount = 0;
				meshlet.vertexCount = 0;
				continue;
			}

			/// record primitive
			for (int k = 0; k < 3; k++)
			{
				int indice = subMeshData->indices[j * 3 + k];
				if (vertexCheckArray[indice] == 0)
				{
					vertexCheckArray[indice] = meshlet.vertexCount;
					vertexIndices.push_back(indice);
					primitiveIndices.push_back(meshlet.vertexCount);
					meshlet.vertexCount++;
				}
				else
				{
					primitiveIndices.push_back(vertexCheckArray[indice]);
				}
			}
			meshlet.primCount += 3;
		}
		meshlets.push_back(meshlet);
		subMeshData->mnum = meshlets.size();

		/// detail buffers
		vRenderer->CreateLocalStorageBufferWithData((void*)meshlets.data(), sizeof(Meshlet) * meshlets.size(), subMeshData->mb, subMeshData->mbm, subMeshData->mbi);
		vRenderer->CreateLocalStorageBufferWithData((void*)vertexIndices.data(), sizeof(uint32_t) * vertexIndices.size(), subMeshData->vib, subMeshData->vibm, subMeshData->vibi);
		vRenderer->CreateLocalStorageBufferWithData((void*)primitiveIndices.data(), sizeof(uint32_t) * primitiveIndices.size(), subMeshData->pib, subMeshData->pibm, subMeshData->pibi);

		/// desc sets
		subMeshData->dsets = new VkDescriptorSet[3];
		vRenderer->AllocateMeshletDescriptorSets(subMeshData->dsets);

		/// upload mesh let here
		vRenderer->UploadMeshlets(&meshData->vsbi, &subMeshData->mbi, &subMeshData->vibi, &subMeshData->pibi, subMeshData->dsets);
	}
}