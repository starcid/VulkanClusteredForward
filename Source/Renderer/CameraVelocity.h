#ifndef __CAMERA_VELOCITY_H__
#define	__CAMERA_VELOCITY_H__

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

class CameraVelocity : public Effect
{
public:
	CameraVelocity(Renderer* pRenderer);
	virtual ~CameraVelocity();

	virtual void Process();

private:
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
	ComPtr<ID3D12Resource> m_cameraVelocity[Renderer::MAX_FRAME_COUNT];
};

#endif // !__CAMERA_VELOCITY_H__
