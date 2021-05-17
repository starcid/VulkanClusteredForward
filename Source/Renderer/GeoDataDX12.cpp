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

	ComPtr<ID3D12Resource> vertex_buffer;
	dRenderer->CreateVertexBuffer((void*)vertices.data(), sizeof(vertices[0]), vertices.size(), vertex_buffer);
	vertex_buffers.push_back(vertex_buffer);

	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
	vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(Vertex);
	vertex_buffer_view.SizeInBytes = sizeof(vertices[0]) * vertices.size();
	vertex_buffer_views.push_back(vertex_buffer_view);
}

void GeoDataDX12::initTinyObjData(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials)
{
	// todo
}