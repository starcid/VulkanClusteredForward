#include "GeoDataDX12.h"

GeoDataDX12::GeoDataDX12(Renderer* renderer)
	:GeoData(renderer)
{

}

GeoDataDX12::~GeoDataDX12()
{
}

void GeoDataDX12::initTestData()
{
	D12Renderer* dRenderer = (D12Renderer*)m_pRenderer;

	MeshData tempMeshData;
	meshDatas.push_back(tempMeshData);
	MeshData& meshData = meshDatas[0];

	dRenderer->CreateVertexBuffer((void*)test_vertices.data(), sizeof(test_vertices[0]), test_vertices.size(), meshData.vb, meshData.vbu, meshData.vbv);
	meshData.vertexs = test_vertices;

	SubMeshData tempSubMeshData;
	meshData.subMeshes.push_back(tempSubMeshData);
	SubMeshData& subMeshData = meshData.subMeshes[0];

	dRenderer->CreateIndexBuffer((void*)test_indices.data(), sizeof(test_indices[0]), test_indices.size(), subMeshData.ib, subMeshData.ibu, subMeshData.ibv);
	subMeshData.indices = test_indices;
}

void GeoDataDX12::initTinyObjData(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials)
{
	D12Renderer* dRenderer = (D12Renderer*)m_pRenderer;

	/// multi vertex buffers
	bool hasWeight = attrib.vertex_weights.size() > 0;
	bool hasWs = attrib.texcoord_ws.size() > 0;

	// pre alloc size to prevent address change
	meshDatas.resize(shapes.size());

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
		MeshData& meshData = meshDatas[i];
		std::vector<Vertex>& vertexs = meshData.vertexs;
		std::map<int, int> indexMap;

		// pre alloc size to prevent address change
		meshData.subMeshes.resize(subMeshMatIds.size());

		///  vertex / indices buffer generate
		int startVtxIdx = 0;
		for (int k = 0; k < subMeshMatIds.size(); k++)
		{
			/// sub mesh data
			SubMeshData& subMeshData = meshData.subMeshes[k];
			std::vector<int>& indices = subMeshData.indices;
			int indicesNum = (subMeshTriIdxs[k] - startVtxIdx) * 3;

			for (int j = 0; j < indicesNum; j++)
			{
				int hashIdx[3];
				Vertex vertex;
				int idx;
				if( j % 3 == 1)
					idx = mesh->indices[startVtxIdx * 3 + j + 1].vertex_index;
				else if(j % 3 == 2)
					idx = mesh->indices[startVtxIdx * 3 + j - 1].vertex_index;
				else
					idx = mesh->indices[startVtxIdx * 3 + j].vertex_index;
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

				if (j % 3 == 1)
					idx = mesh->indices[startVtxIdx * 3 + j + 1].texcoord_index;
				else if (j % 3 == 2)
					idx = mesh->indices[startVtxIdx * 3 + j - 1].texcoord_index;
				else
					idx = mesh->indices[startVtxIdx * 3 + j].texcoord_index;
				vertex.texcoord.x = attrib.texcoords[idx * 2 + 0];
				vertex.texcoord.y = 1.0f - attrib.texcoords[idx * 2 + 1];
				if (hasWs)
					vertex.texcoord.z = attrib.texcoord_ws[idx];
				hashIdx[1] = idx;

				if (j % 3 == 1)
					idx = mesh->indices[startVtxIdx * 3 + j + 1].normal_index;
				else if (j % 3 == 2)
					idx = mesh->indices[startVtxIdx * 3 + j - 1].normal_index;
				else
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
			dRenderer->CreateIndexBuffer((void*)indices.data(), sizeof(indices[0]), indices.size(), subMeshData.ib, subMeshData.ibu, subMeshData.ibv);
		}
		delete[] checkedTangentVtxIdx;

		/// create vertex buffer
		dRenderer->CreateVertexBuffer((void*)vertexs.data(), sizeof(vertexs[0]), vertexs.size(), meshData.vb, meshData.vbu, meshData.vbv);
	}
}