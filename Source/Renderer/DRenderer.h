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
class Texture;
class Material;
class PointLight;
class Effect;
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

	virtual void AddLight(PointLight* light);
	virtual void ClearLight();

	virtual int GetFrameBufferCount() { return frameCount; }
	virtual int GetFrameIndex() { return m_frameIndex; }

	virtual void OnSceneExit();

	void CreateRootSignature(D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, CD3DX12_ROOT_PARAMETER1* rootParameters, int count, ComPtr<ID3D12RootSignature>& rootSignature);

	void CreateVertexBuffer(void* vdata, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& vtxbuf, ComPtr<ID3D12Resource>&bufuploader, D3D12_VERTEX_BUFFER_VIEW& vbv);
	void CreateIndexBuffer(void* idata, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& indicebuf, ComPtr<ID3D12Resource>& bufuploader, D3D12_INDEX_BUFFER_VIEW& ibv);

	void CreateConstBuffer(void** data, uint32_t size, ComPtr<ID3D12Resource>& constbuf, CD3DX12_CPU_DESCRIPTOR_HANDLE& heapHandle);
	void CreateUAVBuffer(void** data, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& uavbuf, CD3DX12_CPU_DESCRIPTOR_HANDLE& heapHandle);
	void CreateSrvUavTexArray(int width, int height, DXGI_FORMAT format, ComPtr<ID3D12Resource>& uavBuf, CD3DX12_CPU_DESCRIPTOR_HANDLE& uavHeapHandle, CD3DX12_CPU_DESCRIPTOR_HANDLE& srvHeapHandle);

	void CreateTexture(void* imageData, int width, int height, DXGI_FORMAT format, ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12Resource>& textureUploadHeap, int& texId);

	void CreateGraphicsPipeLineState(void* vs, int vsSize, void* ps, int psSize, bool depthEnable, bool stencilEnable, D3D12_INPUT_LAYOUT_DESC& layoutDesc, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12PipelineState>& pipelineState);
	void CreateComputePipeLineState(void* cs, int csSize, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12PipelineState>& pipelineState);

	void CreateDescriptorHeap(int count, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, ComPtr<ID3D12DescriptorHeap>& heap);

	inline ComPtr<ID3D12DescriptorHeap>& GetMatCbvHeap() { return m_matCbvHeap; }
	int GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE type);

	void SetRootSignature(ComPtr<ID3D12RootSignature>& rootSignature);
	void SetPipelineState(ComPtr<ID3D12PipelineState>& pipelineState);
	void TransitionResource(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState);
	
	void SetDescriptorHeaps(UINT NumDescriptorHeaps, ID3D12DescriptorHeap* const* ppDescriptorHeaps);
	void SetComputeConstants(UINT RootIndex, DWParam X);
	void SetComputeRootDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE handleStart, int offset, D3D12_DESCRIPTOR_HEAP_TYPE type);
	void SetComputeRootDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE handle);
	void Dispatch2D(size_t ThreadCountX, size_t ThreadCountY, size_t GroupSizeX = 8, size_t GroupSizeY = 8);

	ComPtr<ID3D12Resource>& GetDepthStencil() { return m_depthStencil; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetDepthStencilGpuHandle();
	ComPtr<ID3D12DescriptorHeap>& GetSrvHeap() { return m_srvHeap; }

private:
	struct VertexBufferCreateInfo
	{
		// input
		void* vdata;
		uint32_t single;
		uint32_t length;

		// output
		ComPtr<ID3D12Resource>* pVtxBuf;
		ComPtr<ID3D12Resource>* pBufUploader;
		D3D12_VERTEX_BUFFER_VIEW* pVbv;
	};
	std::vector<D12Renderer::VertexBufferCreateInfo> m_vertexBufferCreateInfos;

	struct IndexBufferCreateInfo
	{
		// input
		void* idata;
		uint32_t single;
		uint32_t length;

		// output
		ComPtr<ID3D12Resource>* pIndicebuf;
		ComPtr<ID3D12Resource>* pBufUploader;
		D3D12_INDEX_BUFFER_VIEW* pIbv;
	};
	std::vector<D12Renderer::IndexBufferCreateInfo> m_indexBufferCreateInfos;

	struct TextureCreateInfo
	{
		// input
		void* imageData;
		int width;
		int height;
		DXGI_FORMAT format;

		// output
		ComPtr<ID3D12Resource>* pTexture;
		ComPtr<ID3D12Resource>* pTextureUpload;
		int* pTexId;
	};
	std::vector<D12Renderer::TextureCreateInfo> m_textureCreateInfos;

private:
	void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = false);
	void CreateDefaultBuffer(const void* initData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& buffer, ComPtr<ID3D12Resource>& uploadBuffer);

	void SetMvpMatrix(glm::mat4x4& mvpMtx);
	void SetModelMatrix(glm::mat4x4& mtx);
	void SetViewMatrix(glm::mat4x4& mtx);
	void SetProjMatrix(glm::mat4x4& mtx);
	void SetProjViewMatrix(glm::mat4x4& mtx);
	void SetCamPos(glm::vec3& pos);
	void SetLightPos(glm::vec4& pos, int idx);
	void SetTexture(Texture* tex);
	void SetNormalTexture(Texture* tex);
	void UpdateMaterial(Material* mat);

	DXGI_FORMAT GetDSVFormat(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT GetDepthFormat(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT GetStencilFormat(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT GetUAVFormat(DXGI_FORMAT defaultFormat);
	DXGI_FORMAT GetBaseFormat(DXGI_FORMAT defaultFormat);

	D3D12_RESOURCE_DESC DescribeGPUBuffer(UINT bufSize);
	D3D12_RESOURCE_DESC DescribeTex2D(uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize, uint32_t NumMips, DXGI_FORMAT Format);

	int CalcConstantBufferByteSize(int byteSize);

	void CreateResources();

	void PostProcess();

private:
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;

	ComPtr<ID3D12CommandQueue> m_graphicsQueue;
	ComPtr<ID3D12CommandQueue> m_computeQueue;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
	ComPtr<ID3D12DescriptorHeap> m_matCbvHeap;
	ComPtr<ID3D12DescriptorHeap> m_uavHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[frameCount];
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12Resource> m_renderTargets[frameCount];
	ComPtr<ID3D12Resource> m_depthStencil;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	CD3DX12_RESOURCE_BARRIER m_transtionBarrier;

	ComPtr<ID3D12Resource> m_transformConstBuffer[frameCount];
	void* m_transformConstBufferBegin[frameCount];
	ComPtr<ID3D12Resource> m_lightConstBuffer[frameCount];
	void* m_lightConstBufferBegin[frameCount];
	ComPtr<ID3D12Resource> m_globalLightIndexUAVBuffer[frameCount];
	void* m_globalLightIndexUAVBufferBegin[frameCount];
	ComPtr<ID3D12Resource> m_lightGridUAVBuffer[frameCount];
	void* m_lightGridUAVBufferBegin[frameCount];

	int m_texCount;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	Texture* m_diffuseTex;
	Texture* m_normalTex;

	int m_descSize[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	int m_frameIndex;
	int m_lastFrameIndex;

	// Synchronization objects.
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[frameCount];

	bool m_bTaa;

	std::vector<Effect*> m_effects;
};

#endif // !__D12_RENDERER_H__
