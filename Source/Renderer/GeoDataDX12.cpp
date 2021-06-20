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

	MeshData meshData;

	dRenderer->CreateVertexBuffer((void*)vertices.data(), sizeof(vertices[0]), vertices.size(), meshData.vb, meshData.vbu, meshData.vbv);
	meshData.vertexs = vertices;

	SubMeshData subMeshData;
	dRenderer->CreateIndexBuffer((void*)indices.data(), sizeof(indices[0]), indices.size(), subMeshData.ib, subMeshData.ibu, subMeshData.ibv);
	subMeshData.indices = indices;

	meshData.subMeshes.push_back(subMeshData);

	meshDatas.push_back(meshData);
}

void GeoDataDX12::initTinyObjData(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials)
{
	// todo
}