#ifndef __D12_RENDERER_H__
#define	__D12_RENDERER_H__

#include <vector>
#include <set>
#include <array>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Renderer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN            // Exclude rarely-used stuff from Windows headers.
#endif

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

class GeoData;
class D12Renderer : public Renderer
{
	static const UINT frameCount = 3;
public:
	D12Renderer(GLFWwindow* win);
	virtual ~D12Renderer();

	virtual void RenderBegin();
	virtual void RenderEnd();
	virtual void Flush();
	virtual void WaitIdle();

	virtual GeoData* CreateGeoData();
	virtual void Draw(GeoData* geoData, std::vector<Material*>& mats);

	virtual void UpdateCameraMatrix();
	virtual void UpdateTransformMatrix(TransformEntity* transform);

	virtual void OnSceneExit();

	void CreateVertexBuffer(void* vdata, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& vtxbuf);

private:
	void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = false);

private:
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;

	ComPtr<ID3D12CommandQueue> m_graphicsQueue;
	ComPtr<ID3D12CommandQueue> m_computeQueue;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvSrvHeap;
	ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12PipelineState> m_pipelineStateShadowMap;
	ComPtr<ID3D12Resource> m_renderTargets[frameCount];
	ComPtr<ID3D12Resource> m_depthStencil;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	int m_rtvDescriptorSize;
	int m_frameIndex;
};

#endif // !__D12_RENDERER_H__
