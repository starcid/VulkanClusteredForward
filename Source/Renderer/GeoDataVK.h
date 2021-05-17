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
	void GenerateMeshlets(void* vtxData, int vtxNum);

private:
	/// renderering data
	std::vector<VkBuffer> vertex_buffers;
	std::vector<VkDeviceMemory> vertex_buffer_memorys;
	std::vector<VkBuffer> index_buffers;
	std::vector<VkDeviceMemory> index_buffer_memorys;
	std::vector<uint32_t> indices_counts;
	std::vector<int32_t> mat_ids;

	/// meshlet data
	std::vector<uint32_t> meshlet_nums;
	std::vector<VkBuffer> meshlet_buffers;
	std::vector<VkDeviceMemory> meshlet_buffers_memorys;
	std::vector<VkDescriptorBufferInfo> meshlet_buffer_infos;
	std::vector<VkBuffer> vertex_storage_buffers;
	std::vector<VkDeviceMemory> vertex_storage_buffer_memorys;
	std::vector<VkDescriptorBufferInfo> vertex_storage_buffer_infos;

	std::vector<VkDescriptorSet*> desc_sets_data;
};

#endif	/*__GEO_DATA_VK_H__*/