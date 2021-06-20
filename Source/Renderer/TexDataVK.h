#ifndef __TEX_DATA_VK_H__
#define __TEX_DATA_VK_H__

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Texture.h"

class TextureDataVK : public TextureData
{
public:
	TextureDataVK(std::string& path);
	virtual ~TextureDataVK();

	inline VkImageView& GetImageView() { return texture_image_view; }
	inline VkSampler& GetTextureSampler() { return texture_sampler; }
	inline VkDescriptorImageInfo* GetImageInfo() { return &image_info; }

private:
	VkImage texture_image;
	VkDeviceMemory texture_image_memory;
	VkImageView texture_image_view;
	VkSampler texture_sampler;
	VkDescriptorImageInfo image_info;
};

#endif // !__TEX_DATA_VK_H__
