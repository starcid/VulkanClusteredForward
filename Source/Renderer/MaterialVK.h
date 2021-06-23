#ifndef __MATERIAL_VK_H__
#define __MATERIAL_VK_H__

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Material.h"

class MaterialVK : public Material
{
public:
	MaterialVK();
	virtual ~MaterialVK();

	inline VkDescriptorSet* GetDescriptorSets() { return desc_sets; }

	inline VkDescriptorBufferInfo* GetBufferInfo() { return &material_uniform_buffer_info; }

protected:
	virtual void InitPlatform();

private:
	VkDescriptorSet desc_sets[3];

	VkBuffer material_uniform_buffer;
	VkDeviceMemory material_uniform_buffer_memory;
	VkDescriptorBufferInfo material_uniform_buffer_info;
	void* material_uniform_buffer_data;
};

#endif