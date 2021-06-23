#include "Application/Application.h"
#include "Renderer/VRenderer.h"
#include "MaterialVK.h"

MaterialVK::MaterialVK()
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)Application::Inst()->GetRenderer();
	vRenderer->AllocateDescriptorSets(desc_sets);

	/// material uniform buffer
	VkDeviceSize bufferSize = sizeof(MaterialData);
	vRenderer->CreateUniformBuffer(&material_uniform_buffer_data, (uint32_t)bufferSize, material_uniform_buffer, material_uniform_buffer_memory);
	material_uniform_buffer_info.buffer = material_uniform_buffer;
	material_uniform_buffer_info.offset = 0;
	material_uniform_buffer_info.range = bufferSize;
}

MaterialVK::~MaterialVK()
{
	VulkanRenderer* vRenderer = (VulkanRenderer*)Application::Inst()->GetRenderer();
	vRenderer->UnmapBufferMemory(material_uniform_buffer_memory);
	vRenderer->CleanBuffer(material_uniform_buffer, material_uniform_buffer_memory);
	vRenderer->FreeDescriptorSets(desc_sets);
}

void MaterialVK::InitPlatform()
{
	MaterialData* matData = (MaterialData*)material_uniform_buffer_data;
	matData->has_albedo_map = has_albedo_map;
	matData->has_normal_map = has_normal_map;
}