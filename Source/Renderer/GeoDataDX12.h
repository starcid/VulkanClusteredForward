/*
	Geometry data for directx12
*/

#ifndef __GEO_DATA_DX12_H__
#define __GEO_DATA_DX12_H__

#include "GeoData.h"
#include "DRenderer.h"

class GeoDataDX12 : public GeoData
{
	friend class D12Renderer;
public:
	GeoDataDX12(Renderer* renderer);
	virtual ~GeoDataDX12();

	virtual void initTestData();
	virtual void initTinyObjData(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials);

private:
	/// struct
	struct SubMeshData
	{
		ComPtr<ID3D12Resource> ib;
		ComPtr<ID3D12Resource> ibu;
		D3D12_INDEX_BUFFER_VIEW ibv;

		std::vector<int> indices;

		int32_t mid;
	};

	struct MeshData
	{
		ComPtr<ID3D12Resource> vb;
		ComPtr<ID3D12Resource> vbu;
		D3D12_VERTEX_BUFFER_VIEW vbv;

		std::vector<Vertex> vertexs;

		std::vector<SubMeshData> subMeshes;
	};

private:
	/// renderering data
	std::vector<MeshData> meshDatas;
};

#endif	/*__GEO_DATA_DX12_H__*/