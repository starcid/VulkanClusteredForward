/*
	Geometry data for directx12
*/

#ifndef __GEO_DATA_DX12_H__
#define __GEO_DATA_DX12_H__

#include "GeoData.h"
#include "DRenderer.h"

class GeoDataDX12 : public GeoData
{
public:
	GeoDataDX12(Renderer* renderer);
	virtual ~GeoDataDX12();

	virtual void initTestData();
	virtual void initTinyObjData(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials);

private:
	std::vector<ComPtr<ID3D12Resource>> vertex_buffers;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> vertex_buffer_views;

};

#endif	/*__GEO_DATA_DX12_H__*/