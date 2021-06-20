#include "TexDataVK.h"

#include "Application/Application.h"
#include "Renderer/VRenderer.h"

TextureDataVK::TextureDataVK(std::string& path)
	:TextureData(path)
{
	VkBuffer buffer;
	VkDeviceMemory mem;
	VulkanRenderer* vRenderer = (VulkanRenderer*)Application::Inst()->GetRenderer();
	uint32_t texSize = GetWidth() * GetHeight() * 4;
	vRenderer->CreateImageBuffer(pixels, texSize, buffer, mem);

	if (!SaveOriginalPixel)
	{
		if (pixels != NULL)
		{
			stbi_image_free(pixels);
			pixels = NULL;
		}
	}
	vRenderer->CreateTextureSampler(&texture_sampler);
	vRenderer->CreateImage(GetWidth(), GetHeight(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture_image, texture_image_memory);

	vRenderer->TransitionImageLayout(texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vRenderer->CopyBufferToImage(buffer, texture_image, static_cast<uint32_t>(GetWidth()), static_cast<uint32_t>(GetHeight()));
	vRenderer->TransitionImageLayout(texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vRenderer->CleanBuffer(buffer, mem);
	texture_image_view = vRenderer->CreateImageView(texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = texture_image_view;
	image_info.sampler = texture_sampler;
}

TextureDataVK::~TextureDataVK()
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)Application::Inst()->GetRenderer();
	vRenderer->DestroyTextureSampler(&texture_sampler);
	vRenderer->CleanImage(texture_image, texture_image_memory, texture_image_view);
}