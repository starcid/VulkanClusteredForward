#ifndef __LINEARIZE_DEPTH_H__
#define	__LINEARIZE_DEPTH_H__

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

#include "Effect.h"

class LinearizeDepth : public Effect
{
public:
	LinearizeDepth(Renderer* pRenderer);
	virtual ~LinearizeDepth();

	virtual void Process();

private:
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
	ComPtr<ID3D12Resource> m_linearDepth[Renderer::MAX_FRAME_COUNT];
};

#endif // !__LINEARIZE_DEPTH_H__
