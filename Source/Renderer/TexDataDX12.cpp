#include "TexDataDX12.h"

#include "Application/Application.h"
#include "Renderer/DRenderer.h"

TextureDataDX12::TextureDataDX12(std::string& path)
	:TextureData(path)
{
    D12Renderer* dRenderer = (D12Renderer*)Application::Inst()->GetRenderer();

    tex_id = dRenderer->CreateTexture(pixels, tex_width, tex_height, DXGI_FORMAT_R8G8B8A8_UNORM, m_texture);
}

TextureDataDX12::~TextureDataDX12()
{
}