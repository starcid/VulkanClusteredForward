#ifndef __MATERIAL_DX12_H__
#define	__MATERIAL_DX12_H__

#include "Material.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include <string>
#include <wrl.h>
#include <process.h>
#include <shellapi.h>
#include <stdexcept>

#include "d3dx12.h"	// help structure

using namespace DirectX;
using namespace Microsoft::WRL;
using Microsoft::WRL::ComPtr;

class MaterialDX12 : public Material
{
public:
	MaterialDX12();
	virtual ~MaterialDX12();

protected:
	virtual void InitPlatform();

private:
	ComPtr<ID3D12Resource> m_matConstBuffer;
	void* m_matConstBufferBegin;
};

#endif // !__MATERIAL_DX12_H__
