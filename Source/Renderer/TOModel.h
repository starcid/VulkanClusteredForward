#ifndef __TO_MODEL_H__
#define __TO_MODEL_H__

#include <tiny_obj_loader.h>
/*view https://github.com/syoyo/tinyobjloader for more informations*/

#include "Material.h"
#include "Model.h"

class TOModel : public Model
{
public:
	TOModel();
	virtual ~TOModel();

	virtual bool LoadFromPath(std::string path);
	virtual void Draw();

	bool LoadTestData();	/// test usage

private:
	void GenerateMeshlets(void* vtxData, int vtxNum);

private:
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	/// material instance
	std::vector<Material*> material_insts;

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
};

#endif // !__TO_MODEL_H__
