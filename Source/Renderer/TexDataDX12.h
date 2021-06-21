#ifndef __TEX_DATA_DX12_H__
#define __TEX_DATA_DX12_H__

#include "Texture.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN            // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"	// help structure

using namespace DirectX;
using namespace Microsoft::WRL;
using Microsoft::WRL::ComPtr;

class TextureDataDX12 : public TextureData
{
public:
	TextureDataDX12(std::string& path);
	virtual ~TextureDataDX12();

	inline ComPtr<ID3D12Resource>& GetTexture() { return m_texture; }

private:
	ComPtr<ID3D12Resource> m_texture;
};

#endif // !__TEX_DATA_DX12_H__
