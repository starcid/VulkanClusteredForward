#include "Application/Application.h"
#include "Common/Utils.h"
#include "Camera.h"
#include "Texture.h"
#include "Material.h"
#include "Light.h"

#include "DRenderer.h"

#include "ClusteCulling.h"
#include "GeoDataDX12.h"
#include "TexDataDX12.h"

inline std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}

class HrException : public std::runtime_error
{
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw HrException(hr);
    }
}

#if defined(_DEBUG) || defined(DBG)
inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
    pObject->SetName(name);
}
inline void SetNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
    WCHAR fullName[50];
    if (swprintf_s(fullName, L"%s[%u]", name, index) > 0)
    {
        pObject->SetName(fullName);
    }
}
#else
inline void SetName(ID3D12Object*, LPCWSTR)
{
}
inline void SetNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}
#endif

#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed((x)[n].Get(), L#x, n)

const D3D12_INPUT_ELEMENT_DESC StandardVertexDescription[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, pos),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, texcoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, tangent), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

enum RootParameterIndex
{
    CbvParameter,       //b0,b1,b2
    UavParameter,       //u0,u1
    SrvParameter0,      //t0
    SrvParameter1,      //t1
    SamplerParameter,   //s0,s1
    RootParameterCount,
};

D12Renderer::D12Renderer(GLFWwindow* win)
	:Renderer(win)
{
	HWND hwnd = glfwGetWin32Window(win);
    UINT dxgiFactoryFlags = 0;
    m_texCount = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    /// create device
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (0)  /// not using warp
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter, true);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }

    /// Queue ( we use graphics queue as compute queue if possible )
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_graphicsQueue)));
    NAME_D3D12_OBJECT(m_graphicsQueue);
    m_computeQueue = m_graphicsQueue; /// make sure later ... 

    /// swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = frameCount;  /// include front buffer already
    swapChainDesc.Width = winWidth;
    swapChainDesc.Height = winHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  /// common format ?
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_graphicsQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    // Detect if newly created full-screen swap chain isn't actually full screen.
    IDXGIOutput* pTarget; 
    BOOL bFullscreen;
    if (SUCCEEDED(swapChain->GetFullscreenState(&bFullscreen, &pTarget)))
    {
        pTarget->Release();
    }
    else
        bFullscreen = FALSE;
    // If not full screen, enable full screen again.
    if (!bFullscreen)
    {
        ShowWindow(hwnd, SW_MINIMIZE);
        ShowWindow(hwnd, SW_RESTORE);
        swapChain->SetFullscreenState(TRUE, NULL);
    }

    ThrowIfFailed(swapChain.As(&m_swapChain));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = frameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Describe and create a depth stencil view (DSV) descriptor heap.
        // Each frame has its own depth stencils (to write shadows onto) 
        // and then there is one for the scene itself.
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1 + frameCount * 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        m_dsvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        // CBV heap b0 b1 b2, b2 light won't change
        const UINT cbvCount = frameCount * 2 + 1;
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = cbvCount;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
        NAME_D3D12_OBJECT(m_cbvHeap);

        // UAV u0 u1
        const UINT uavCount = frameCount * 2;
        D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {};
        uavHeapDesc.NumDescriptors = uavCount;
        uavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        uavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&m_uavHeap)));
        NAME_D3D12_OBJECT(m_uavHeap);

        // SRV t0 t1
        const UINT srvCount = MAX_MATERIAL_NUM * 2 + 1; // normal & diffuse + default
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = srvCount;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
        NAME_D3D12_OBJECT(m_srvHeap);

        m_cbvUavSrvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // sample s0 s1
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.NumDescriptors = 2;
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));
        NAME_D3D12_OBJECT(m_samplerHeap);
        m_samplerDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    // Create samplers
    {
        /// also can use static desc in root signature, here seperate
        D3D12_SAMPLER_DESC sampleDesc = {};
        sampleDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampleDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampleDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampleDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampleDesc.MipLODBias = 0;
        sampleDesc.MaxAnisotropy = 1;
        sampleDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        for (int i = 0; i < 4; i++)
            sampleDesc.BorderColor[i] = 0.0f;
        sampleDesc.MinLOD = 0.0f;
        sampleDesc.MaxLOD = D3D12_FLOAT32_MAX;

        CD3DX12_CPU_DESCRIPTOR_HANDLE hSamplerHeap(m_samplerHeap->GetCPUDescriptorHandleForHeapStart());
        for (int i = 0; i < 2; i++)
        {
            m_device->CreateSampler(&sampleDesc, hSamplerHeap);
            hSamplerHeap.Offset(m_samplerDescSize);
        }
    }

    // Create const buffers
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCbvHeap(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
        for (int i = 0; i < frameCount; i++)
        {
            CreateConstBuffer(&m_transformConstBufferBegin[i], sizeof(TransformData), m_transformConstBuffer[i], hCbvHeap);
            hCbvHeap.Offset(m_cbvUavSrvDescSize);
            CreateConstBuffer(&m_materialConstBufferBegin[i], sizeof(MaterialData), m_materialConstBuffer[i], hCbvHeap);
            hCbvHeap.Offset(m_cbvUavSrvDescSize);
            CreateConstBuffer(&m_lightConstBufferBegin[i], sizeof(PointLightData) * MAX_LIGHT_NUM, m_lightConstBuffer[i], hCbvHeap);
            hCbvHeap.Offset(m_cbvUavSrvDescSize);
        }
    }

    // Create UAV buffers
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE hUavHeap(m_uavHeap->GetCPUDescriptorHandleForHeapStart());
        for (int i = 0; i < frameCount; i++)
        {
            CreateUAVBuffer(&m_globalLightIndexUAVBufferBegin[i], sizeof(uint32_t), MAX_LIGHT_NUM * CLUSTE_NUM, m_globalLightIndexUAVBuffer[i], hUavHeap);
            hUavHeap.Offset(m_cbvUavSrvDescSize);
            CreateUAVBuffer(&m_lightGridUAVBufferBegin[i], sizeof(LightGrid), CLUSTE_NUM, m_lightGridUAVBuffer[i], hUavHeap);
            hUavHeap.Offset(m_cbvUavSrvDescSize);
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

    // Create the root signature.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        
        // b0 b1 b2
        D3D12_DESCRIPTOR_RANGE Param0Ranges[1];
        Param0Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        Param0Ranges[0].BaseShaderRegister = 0;
        Param0Ranges[0].NumDescriptors = 3;

        // u0 u1
        D3D12_DESCRIPTOR_RANGE Param1Ranges[2];
        Param1Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        Param1Ranges[0].BaseShaderRegister = 0;
        Param1Ranges[0].NumDescriptors = 2;

        // t0
        D3D12_DESCRIPTOR_RANGE Param2Ranges[1];
        Param2Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        Param2Ranges[0].BaseShaderRegister = 0;
        Param2Ranges[0].NumDescriptors = 1;

        // t1
        D3D12_DESCRIPTOR_RANGE Param3Ranges[1];
        Param3Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        Param3Ranges[0].BaseShaderRegister = 1;
        Param3Ranges[0].NumDescriptors = 1;

        // s0 s1
        D3D12_DESCRIPTOR_RANGE Param4Ranges[1];
        Param4Ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        Param4Ranges[0].BaseShaderRegister = 0;
        Param4Ranges[0].NumDescriptors = 2;

        // A single 32-bit constant root parameter that is used by the vertex shader.
        CD3DX12_ROOT_PARAMETER rootParameters[RootParameterIndex::RootParameterCount];
        rootParameters[RootParameterIndex::CbvParameter].InitAsDescriptorTable(1, Param0Ranges, D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[RootParameterIndex::UavParameter].InitAsDescriptorTable(1, Param1Ranges, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameterIndex::SrvParameter0].InitAsDescriptorTable(1, Param2Ranges, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameterIndex::SrvParameter1].InitAsDescriptorTable(1, Param3Ranges, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameterIndex::SamplerParameter].InitAsDescriptorTable(1, Param4Ranges, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        // Serialize the root signature.
        ComPtr<ID3DBlob> rootSignatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDescription,
            featureData.HighestVersion, &rootSignatureBlob, &errorBlob));
        // Create the root signature.
        ThrowIfFailed(m_device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
            rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // Create the pipeline state, which includes loading shaders.
    // todo: convert shader glsl->hlsl later...
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        ThrowIfFailed(D3DCompileFromFile(L"Data/shader/tinyobj_vert.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(L"Data/shader/tinyobj_frag.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        D3D12_INPUT_LAYOUT_DESC inputLayoutDesc;
        inputLayoutDesc.pInputElementDescs = StandardVertexDescription;
        inputLayoutDesc.NumElements = _countof(StandardVertexDescription);

        CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
        depthStencilDesc.DepthEnable = true;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        depthStencilDesc.StencilEnable = FALSE;

        // Describe and create the PSO for rendering the scene.
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = inputLayoutDesc;
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
        NAME_D3D12_OBJECT(m_pipelineState);
    }

    // Create command list for initial GPU setup.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Create render target views (RTVs).
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < frameCount; i++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);

        NAME_D3D12_OBJECT_INDEXED(m_renderTargets, i);
    }

    // Create the depth stencil.
    {
        CD3DX12_RESOURCE_DESC shadowTextureDesc(
            D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            0,
            static_cast<UINT>(m_viewport.Width),
            static_cast<UINT>(m_viewport.Height),
            1,
            1,
            DXGI_FORMAT_D32_FLOAT,
            1,
            0,
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

        D3D12_CLEAR_VALUE clearValue;    // Performance tip: Tell the runtime at resource creation the desired clear value.
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = clear_depth;
        clearValue.DepthStencil.Stencil = clear_stencil;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &shadowTextureDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_depthStencil)));

        NAME_D3D12_OBJECT(m_depthStencil);

        // Create the depth stencil view.
        m_device->CreateDepthStencilView(m_depthStencil.Get(), nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }
}

D12Renderer::~D12Renderer()
{
	
}

void D12Renderer::RenderBegin()
{
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // bind root signature with descript heaps
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get(), m_uavHeap.Get(), m_srvHeap.Get(), m_samplerHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // b0 b1 b2
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), m_frameIndex * 3, m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::CbvParameter, cbvHandle);

    // u0 u1
    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle(m_uavHeap->GetGPUDescriptorHandleForHeapStart(), m_frameIndex * 2, m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::UavParameter, uavHandle);

    // s0 s1
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::SamplerParameter, m_samplerHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_transtionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &m_transtionBarrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Clear the render targets.
    m_commandList->ClearRenderTargetView(rtvHandle, clear_color, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, clear_depth, clear_stencil, 0, nullptr);

    /// default triangle list
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D12Renderer::RenderEnd()
{
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &m_transtionBarrier);
    m_transtionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    ThrowIfFailed(m_commandList->Close());
}

void D12Renderer::Flush()
{
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D12Renderer::WaitIdle()
{
	
}

GeoData* D12Renderer::CreateGeoData()
{
    return new GeoDataDX12(this);
}

void D12Renderer::Draw(GeoData* geoData, std::vector<Material*>& mats)
{
    D12Renderer* dRenderer = this;
    GeoDataDX12* data = (GeoDataDX12*)geoData;

    /// prepare materials
    for (int i = 0; i < mats.size(); i++)
    {
        mats[i]->PrepareToDraw();
    }

    /// draw with indice buffer
    for (int i = 0; i < data->meshDatas.size(); i++)
    {
        GeoDataDX12::MeshData* meshData = &data->meshDatas[i];

        m_commandList->IASetVertexBuffers(0, 1, &meshData->vbv);

        for (int j = 0; j < meshData->subMeshes.size(); j++)
        {
            GeoDataDX12::SubMeshData* subMeshData = &meshData->subMeshes[j];

            if (subMeshData->mid >= 0 && mats[subMeshData->mid] != NULL)
            {
                /// material
                Material* mat = mats[subMeshData->mid];
                dRenderer->SetTexture(mat->GetDiffuseTexture());
                dRenderer->SetNormalTexture(mat->GetNormalTexture());
                dRenderer->UpdateMaterial(mat);
            }

            m_commandList->IASetIndexBuffer(&subMeshData->ibv);
            m_commandList->DrawIndexedInstanced(subMeshData->indices.size(), 1, 0, 0, 0);
        }
    }
}

void D12Renderer::UpdateCameraMatrix()
{
    SetViewMatrix(*camera->GetViewMatrix());
    SetProjMatrix(*camera->GetProjectMatrix());
    SetProjViewMatrix(*camera->GetViewProjectMatrix());
}

void D12Renderer::UpdateTransformMatrix(TransformEntity* transform)
{
    glm::mat4x4* modelViewMatrix = transform->UpdateMatrix();
    glm::mat4x4 mvp = (*camera->GetViewProjectMatrix()) * (*modelViewMatrix);
    SetMvpMatrix(mvp);
    SetModelMatrix(*modelViewMatrix);

    glm::mat4 world_model = glm::inverse(*modelViewMatrix);
    glm::vec3 cam_pos = camera->GetPosition();
    glm::vec4 cam_pos_obj = glm::vec4(cam_pos.x, cam_pos.y, cam_pos.z, 1.0f) * world_model;
    glm::vec3 cam_pos_obj3 = glm::vec3(cam_pos_obj.x, cam_pos_obj.y, cam_pos_obj.z);
    SetCamPos(cam_pos_obj3);

    for (int i = 0; i < light_infos.size(); i++)
    {
        glm::vec4 light_pos_obj = glm::vec4(light_infos[i].pos.x, light_infos[i].pos.y, light_infos[i].pos.z, 1.0f) * world_model;
        SetLightPos(light_pos_obj, i);
    }
}

void D12Renderer::AddLight(PointLight* light)
{
    Renderer::AddLight(light);

    int idx = light_infos.size() - 1;
    PointLightData& lightData = light_infos[idx];
    for (int i = 0; i < frameCount; i++)
    {
        memcpy((PointLightData*)m_lightConstBufferBegin[i] + idx, &lightData, sizeof(PointLightData));
    }
}

void D12Renderer::ClearLight()
{
    Renderer::ClearLight();
}

void D12Renderer::OnSceneExit()
{
    ClearLight();
}

void D12Renderer::SetMvpMatrix(glm::mat4x4& mvpMtx)
{
    TransformData* transData = (TransformData*)m_transformConstBufferBegin[m_frameIndex];
    memcpy(&transData->mvp, &mvpMtx, sizeof(glm::mat4x4));
}

void D12Renderer::SetModelMatrix(glm::mat4x4& mtx)
{
    TransformData* transData = (TransformData*)m_transformConstBufferBegin[m_frameIndex];
    memcpy(&transData->model, &mtx, sizeof(glm::mat4x4));
}

void D12Renderer::SetViewMatrix(glm::mat4x4& mtx)
{
    TransformData* transData = (TransformData*)m_transformConstBufferBegin[m_frameIndex];
    memcpy(&transData->view, &mtx, sizeof(glm::mat4x4));
}

void D12Renderer::SetProjMatrix(glm::mat4x4& mtx)
{
    TransformData* transData = (TransformData*)m_transformConstBufferBegin[m_frameIndex];
    memcpy(&transData->proj, &mtx, sizeof(glm::mat4x4));
}

void D12Renderer::SetProjViewMatrix(glm::mat4x4& mtx)
{
    TransformData* transData = (TransformData*)m_transformConstBufferBegin[m_frameIndex];
    memcpy(&transData->proj_view, &mtx, sizeof(glm::mat4x4));
}

void D12Renderer::SetCamPos(glm::vec3& pos)
{
    float zNear = camera->GetNearDistance();
    float zFar = camera->GetFarDistance();
    TransformData* transData = (TransformData*)m_transformConstBufferBegin[m_frameIndex];
    memcpy(&transData->cam_pos, &pos, sizeof(glm::vec3));
    transData->zNear = zNear;
    transData->zFar = zFar;
    transData->scale = (float)CLUSTE_Z / std::log2f(zFar / zNear);
    transData->bias = -((float)CLUSTE_Z * std::log2f(zNear) / std::log2f(zFar / zNear));
    transData->isClusteShading = false;
}

void D12Renderer::SetLightPos(glm::vec4& pos, int idx)
{
    TransformData* transData = (TransformData*)m_transformConstBufferBegin[m_frameIndex];
    memcpy(&transData->light_pos[idx], &pos, sizeof(glm::vec4));
}

void D12Renderer::SetTexture(Texture* tex)
{
    if (tex == NULL)
        m_diffuseTex = default_tex;
    else
        m_diffuseTex = tex;
}

void D12Renderer::SetNormalTexture(Texture* tex)
{
    if (tex == NULL)
        m_normalTex = default_tex;
    else
        m_normalTex = tex;
}

void D12Renderer::UpdateMaterial(Material* mat)
{
    //  material update
    MaterialData* matData = (MaterialData*)m_materialConstBufferBegin[m_frameIndex];
    matData->has_albedo_map = mat->GetHasAlbedoMap();
    matData->has_normal_map = mat->GetHasNormalMap();
    
    // t0 t1
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvT0Handle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), m_diffuseTex->GetTextureData()->GetTexId(), m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::SrvParameter0, srvT0Handle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvT1Handle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), m_normalTex->GetTextureData()->GetTexId(), m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::SrvParameter1, srvT1Handle);
}

int D12Renderer::CreateTexture(void* imageData, int width, int height, DXGI_FORMAT format, ComPtr<ID3D12Resource>& texture)
{
    ComPtr<ID3D12Resource> textureUploadHeap;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = format;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&texture)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);

    // Create the GPU upload buffer.
    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&textureUploadHeap)));

    // Copy data to the intermediate upload heap and then schedule a copy 
    // from the upload heap to the Texture2D.
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = imageData;
    textureData.RowPitch = width * 4;
    textureData.SlicePitch = textureData.RowPitch * height;

    UpdateSubresources(m_commandList.Get(), texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);

    D3D12_RESOURCE_BARRIER resBarrier = CD3DX12_RESOURCE_BARRIER::Transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &resBarrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE hSrvHeap(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), m_texCount, m_cbvUavSrvDescSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(texture.Get(), &srvDesc, hSrvHeap);

    m_texCount++;
    return m_texCount - 1;
}

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_
void D12Renderer::GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter));
            ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }
    else
    {
        for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}

void D12Renderer::CreateVertexBuffer(void* vdata, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& vtxbuf, ComPtr<ID3D12Resource>& bufuploader, D3D12_VERTEX_BUFFER_VIEW& vbv)
{
    const UINT vertexBufferSize = single * length;
    vtxbuf = CreateDefaultBuffer(m_device, m_commandList, vdata, vertexBufferSize, bufuploader);

    vbv.BufferLocation = vtxbuf->GetGPUVirtualAddress();
    vbv.StrideInBytes = single;
    vbv.SizeInBytes = vertexBufferSize;
}

void D12Renderer::CreateIndexBuffer(void* idata, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& indicebuf, ComPtr<ID3D12Resource>& bufuploader, D3D12_INDEX_BUFFER_VIEW& ibv)
{
    const UINT indiceBufferSize = single * length;
    indicebuf = CreateDefaultBuffer(m_device, m_commandList, idata, indiceBufferSize, bufuploader);

    ibv.BufferLocation = indicebuf->GetGPUVirtualAddress();
    ibv.Format = DXGI_FORMAT_R32_UINT;
    ibv.SizeInBytes = indiceBufferSize;
}

int D12Renderer::CalcConstantBufferByteSize(int byteSize)
{
    return (byteSize + 255) & (~255);
}

void D12Renderer::CreateConstBuffer(void** data, uint32_t size, ComPtr<ID3D12Resource>& constbuf, CD3DX12_CPU_DESCRIPTOR_HANDLE& heapHandle)
{
    const UINT constantBufferSize = CalcConstantBufferByteSize(size);    // CB size is required to be 256-byte aligned.

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&constbuf)));

    // Describe and create a constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = constbuf->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;
    m_device->CreateConstantBufferView(&cbvDesc, heapHandle);

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(constbuf->Map(0, &readRange, reinterpret_cast<void**>(data)));
}

void D12Renderer::CreateUAVBuffer(void** data, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& uavbuf, CD3DX12_CPU_DESCRIPTOR_HANDLE& heapHandle)
{
    const UINT uavBufferSize = single * length;

    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(uavBufferSize);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uavbuf)));

    // Describe and create a constant buffer view.
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.NumElements = length;
    uavDesc.Buffer.StructureByteStride = single;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    m_device->CreateUnorderedAccessView(uavbuf.Get(), nullptr, &uavDesc, heapHandle);

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(uavbuf->Map(0, &readRange, reinterpret_cast<void**>(data)));
}

ComPtr<ID3D12Resource> D12Renderer::CreateDefaultBuffer(ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& cmdList, const void* initData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    // Create the actual default buffer resource.
    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // In order to copy CPU memory data into our default buffer, we need
    // to create an intermediate upload heap.
    heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    resDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    // Describe the data we want to copy into the default buffer.
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // Schedule to copy the data to the default buffer resource.
    // At a high level, the helper function UpdateSubresources
    // will copy the CPU memory into the intermediate upload heap.
    // Then, using ID3D12CommandList::CopySubresourceRegion,
    // the intermediate upload heap data will be copied to mBuffer.
    D3D12_RESOURCE_BARRIER resBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &resBarrier);
    UpdateSubresources<1>(cmdList.Get(),
        defaultBuffer.Get(), uploadBuffer.Get(),
        0, 0, 1, &subResourceData);
    resBarrier = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &resBarrier);

    // Note: uploadBuffer has to be kept alive after the above function
    // calls because the command list has not been executed yet that
    // performs the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy
    // has been executed.
    return defaultBuffer;
}