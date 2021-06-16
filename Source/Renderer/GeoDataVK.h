/*
	Geometry data for vulkan
*/

#ifndef __GEO_DATA_VK_H__
#define __GEO_DATA_VK_H__

#include "GeoData.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

class GeoDataVK : public GeoData
{
	friend class VulkanRenderer;
public:
	GeoDataVK(Renderer* renderer);
	virtual ~GeoDataVK();

	virtual void initTestData();
	virtual void initTinyObjData(tinyobj::attrib_t& attrib, std::vector<tinyobj::shape_t>& shapes, std::vector<tinyobj::material_t>& materials);

private:
	/// struct
	struct SubMeshData
	{
		VkBuffer ib;
		VkDeviceMemory ibm;
		
		std::vector<int> indices;

		int32_t mid;

		///  for meshlets
		uint32_t mnum;
		VkBuffer mb;
		VkDeviceMemory mbm;
		VkDescriptorBufferInfo mbi;
		VkBuffer pib;
		VkDeviceMemory pibm;
		VkDescriptorBufferInfo pibi;
		VkBuffer vib;
		VkDeviceMemory vibm;
		VkDescriptorBufferInfo vibi;
		VkDescriptorSet* dsets;
	};

	struct MeshData
	{
		VkBuffer vb;
		VkDeviceMemory vbm;

		std::vector<Vertex> vertexs;

		std::vector<SubMeshData> subMeshes;

		///  for meshlets
		VkBuffer vsb;
		VkDeviceMemory vsbm;
		VkDescriptorBufferInfo vsbi;
	};

private:
	void GenerateMeshlets(MeshData* meshData);
	int CalculateHash(int idx1, int idx2, int idx3);

	/// renderering data
	std::vector<MeshData> meshDatas;
};

#endif	/*__GEO_DATA_VK_H__*/