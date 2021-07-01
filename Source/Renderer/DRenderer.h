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

	virtual void OnSceneExit();

	void CreateVertexBuffer(void* vdata, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& vtxbuf, ComPtr<ID3D12Resource>&bufuploader, D3D12_VERTEX_BUFFER_VIEW& vbv);
	void CreateIndexBuffer(void* idata, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& indicebuf, ComPtr<ID3D12Resource>& bufuploader, D3D12_INDEX_BUFFER_VIEW& ibv);

	void CreateConstBuffer(void** data, uint32_t size, ComPtr<ID3D12Resource>& constbuf, CD3DX12_CPU_DESCRIPTOR_HANDLE& heapHandle);
	void CreateUAVBuffer(void** data, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& uavbuf, CD3DX12_CPU_DESCRIPTOR_HANDLE& heapHandle);

	void CreateTexture(void* imageData, int width, int height, DXGI_FORMAT format, ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12Resource>& textureUploadHeap, int& texId);

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
	void CreateDefaultBuffer(ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& cmdList, const void* initData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& buffer, ComPtr<ID3D12Resource>& uploadBuffer);

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

	D3D12_RESOURCE_DESC DescribeGPUBuffer(UINT bufSize);

	int CalcConstantBufferByteSize(int byteSize);

	void CreateResources();

private:
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;

	ComPtr<ID3D12CommandQueue> m_graphicsQueue;
	ComPtr<ID3D12CommandQueue> m_computeQueue;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
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
	ComPtr<ID3D12Resource> m_materialConstBuffer[frameCount];
	void* m_materialConstBufferBegin[frameCount];
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

	int m_rtvDescriptorSize;
	int m_samplerDescSize;
	int m_cbvUavSrvDescSize;
	int m_dsvDescSize;

	int m_frameIndex;
	int m_lastFrameIndex;

	// Synchronization objects.
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValues[frameCount];
};

#endif // !__D12_RENDERER_H__
