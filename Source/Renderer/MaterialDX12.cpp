#include "Application/Application.h"
#include "Renderer/DRenderer.h"
#include "MaterialDX12.h"

MaterialDX12::MaterialDX12()
	:Material()
{
}

MaterialDX12::~MaterialDX12()
{
}

void MaterialDX12::InitPlatform()
{
	D12Renderer* dRenderer = (D12Renderer*)Application::Inst()->GetRenderer();

	CD3DX12_CPU_DESCRIPTOR_HANDLE hMatCbvHeap(dRenderer->GetMatCbvHeap()->GetCPUDescriptorHandleForHeapStart(), mat_id, dRenderer->GetCbvUavSrvDescSize());
	dRenderer->CreateConstBuffer(&m_matConstBufferBegin, sizeof(MaterialData), m_matConstBuffer, hMatCbvHeap);

	MaterialData* matData = (MaterialData*)m_matConstBufferBegin;
	matData->has_albedo_map = has_albedo_map;
	matData->has_normal_map = has_normal_map;
}