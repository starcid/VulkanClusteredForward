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
    CbvParameter,       //b0,b1
    MatCbvParameter,    //b2
    UavParameter,       //u1,u2
    SrvParameter0,      //t0
    SrvParameter1,      //t1
    SamplerParameter,   //s0,s1
    RootParameterCount,
};

enum
{
    DepthSRV = 0,
    ReverseSRVCount,
};

D12Renderer::D12Renderer(GLFWwindow* win)
	:Renderer(win)
    ,m_viewport(0.0f, 0.0f, static_cast<float>(winWidth), static_cast<float>(winHeight))
    ,m_scissorRect(0, 0, static_cast<LONG>(winWidth), static_cast<LONG>(winHeight))
    ,m_fenceValues {}
{
    renderer_type = Renderer::DX12;

	HWND hwnd = glfwGetWin32Window(win);
    UINT dxgiFactoryFlags = 0;
    m_texCount = 0;
    m_lastFrameIndex = -1;

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

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
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
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        m_dsvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        // CBV heap b0 b1
        const UINT cbvCount = frameCount * 2;
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = cbvCount;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
        NAME_D3D12_OBJECT(m_cbvHeap);

        // mat cbv b2
        const UINT matCbvCount = MAX_MATERIAL_NUM;
        D3D12_DESCRIPTOR_HEAP_DESC matCbvHeapDesc = {};
        matCbvHeapDesc.NumDescriptors = matCbvCount;
        matCbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        matCbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&matCbvHeapDesc, IID_PPV_ARGS(&m_matCbvHeap)));
        NAME_D3D12_OBJECT(m_matCbvHeap);

        // UAV u1 u2
        const UINT uavCount = frameCount * 2;
        D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {};
        uavHeapDesc.NumDescriptors = uavCount;
        uavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        uavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&m_uavHeap)));
        NAME_D3D12_OBJECT(m_uavHeap);

        // SRV t0 t1
        const UINT srvCount = ReverseSRVCount + MAX_MATERIAL_NUM * 2 + 1; // normal & diffuse + default
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
        sampleDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        for (int i = 0; i < 4; i++)
            sampleDesc.BorderColor[i] = 0.0f;
        sampleDesc.MinLOD = 0.0f;
        sampleDesc.MaxLOD = 0.0f;

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
        CD3DX12_DESCRIPTOR_RANGE1 ParamRanges[6];
        ParamRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0);
        ParamRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
        ParamRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 1);
        ParamRanges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        ParamRanges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
        ParamRanges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0);

        // A single 32-bit constant root parameter that is used by the vertex shader.
        CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameterIndex::RootParameterCount];
        rootParameters[RootParameterIndex::CbvParameter].InitAsDescriptorTable(1, &ParamRanges[0], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[RootParameterIndex::MatCbvParameter].InitAsDescriptorTable(1, &ParamRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameterIndex::UavParameter].InitAsDescriptorTable(1, &ParamRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameterIndex::SrvParameter0].InitAsDescriptorTable(1, &ParamRanges[3], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameterIndex::SrvParameter1].InitAsDescriptorTable(1, &ParamRanges[4], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[RootParameterIndex::SamplerParameter].InitAsDescriptorTable(1, &ParamRanges[5], D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        // Serialize the root signature.
        ComPtr<ID3DBlob> rootSignatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));
        // Create the root signature.
        ThrowIfFailed(m_device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // Create the pipeline state, which includes loading shaders.
    // todo: convert shader glsl->hlsl later...
    {
        //ComPtr<ID3DBlob> vertexShader;
        //ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        //UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        //UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        //ThrowIfFailed(D3DCompileFromFile(L"Data/shader/tinyobj.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        //ThrowIfFailed(D3DCompileFromFile(L"Data/shader/tinyobj.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        std::vector<char> vertexShader = Utils::readFile("Data/shader/tinyobj_vert.cso");
        std::vector<char> pixelShader = Utils::readFile("Data/shader/tinyobj_frag.cso");

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
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.data(), vertexShader.size());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.data(), pixelShader.size());
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

    // Create render target views (RTVs).
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < frameCount; i++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);

        NAME_D3D12_OBJECT_INDEXED(m_renderTargets, i);

        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
        m_fenceValues[i] = 0;
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
    m_commandList->Close();

    // Create the depth stencil.
    {
        DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT;
        CD3DX12_RESOURCE_DESC shadowTextureDesc(
            D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            0,
            static_cast<UINT>(m_viewport.Width),
            static_cast<UINT>(m_viewport.Height),
            1,
            1,
            format,
            1,
            0,
            D3D12_TEXTURE_LAYOUT_UNKNOWN,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        D3D12_CLEAR_VALUE clearValue;    // Performance tip: Tell the runtime at resource creation the desired clear value.
        clearValue.Format = format;
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
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Format = GetDSVFormat(format);
        if (shadowTextureDesc.SampleDesc.Count == 1)
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }
        else
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
        }
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        m_device->CreateDepthStencilView(m_depthStencil.Get(), nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create the depth shader resource view
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = GetDepthFormat(format);
        if (dsvDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
        {
            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = 1;
        }
        else
        {
            SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        }
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), DepthSRV, m_cbvUavSrvDescSize);
        m_device->CreateShaderResourceView(m_depthStencil.Get(), &SRVDesc, srvHandle);
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValues[m_frameIndex]++;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitIdle();
    }
}

D12Renderer::~D12Renderer()
{
    WaitIdle();

    CloseHandle(m_fenceEvent);
}

DXGI_FORMAT D12Renderer::GetDSVFormat(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;

        // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_D16_UNORM;

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT D12Renderer::GetDepthFormat(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

        // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

        // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_UNORM;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT D12Renderer::GetStencilFormat(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

        // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

void D12Renderer::RenderBegin()
{
    Renderer::RenderBegin();

    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

    // Create necessary resources
    CreateResources();

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // bind root signature with descript heaps
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get(), m_uavHeap.Get(), m_samplerHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, ppHeaps);

    // b0 b1
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), m_frameIndex * 2, m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::CbvParameter, cbvHandle);

    m_commandList->SetDescriptorHeaps(1, ppHeaps + 1);

    // u1 u2
    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle(m_uavHeap->GetGPUDescriptorHandleForHeapStart(), m_frameIndex * 2, m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::UavParameter, uavHandle);

    m_commandList->SetDescriptorHeaps(1, ppHeaps + 2);

    // s0 s1
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::SamplerParameter, m_samplerHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_transtionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &m_transtionBarrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Clear the render targets.
    m_commandList->ClearRenderTargetView(rtvHandle, clear_color, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, clear_depth, clear_stencil, 0, nullptr);

    /// default triangle list
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D12Renderer::RenderEnd()
{
    // Indicate that the back buffer will now be used to present.
    m_transtionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &m_transtionBarrier);
    ThrowIfFailed(m_commandList->Close());

    Renderer::RenderEnd();
}

void D12Renderer::Flush()
{
    if (m_lastFrameIndex != -1)
    {
        // Schedule a Signal command in the queue.
        const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
        ThrowIfFailed(m_graphicsQueue->Signal(m_fence.Get(), currentFenceValue));

        // Update the frame index.
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        // If the next frame is not ready to be rendered yet, wait until it is ready.
        if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
        {
            ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
        }

        // Set the fence value for the next frame.
        m_fenceValues[m_frameIndex] = currentFenceValue + 1;
    }

    /// scene renderering
    Application::Inst()->SceneRender();

    /// draw
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_graphicsQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ThrowIfFailed(m_swapChain->Present(0, 0));

    m_lastFrameIndex = m_frameIndex;
}

void D12Renderer::WaitIdle()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_graphicsQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    // Increment the fence value for the current frame.
    m_fenceValues[m_frameIndex]++;
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
    ID3D12DescriptorHeap* ppHeaps[] = { m_matCbvHeap.Get(), m_srvHeap.Get() };

    m_commandList->SetDescriptorHeaps(1, ppHeaps);
    //  b2
    CD3DX12_GPU_DESCRIPTOR_HANDLE matCbvHandle(m_matCbvHeap->GetGPUDescriptorHandleForHeapStart(), mat->GetMatId(), m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::MatCbvParameter, matCbvHandle);
    
    m_commandList->SetDescriptorHeaps(1, ppHeaps + 1);
    // t0 t1
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvT0Handle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), m_diffuseTex->GetTextureData()->GetTexId() + ReverseSRVCount, m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::SrvParameter0, srvT0Handle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvT1Handle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), m_normalTex->GetTextureData()->GetTexId() + ReverseSRVCount, m_cbvUavSrvDescSize);
    m_commandList->SetGraphicsRootDescriptorTable(RootParameterIndex::SrvParameter1, srvT1Handle);
}

void D12Renderer::CreateTexture(void* imageData, int width, int height, DXGI_FORMAT format, ComPtr<ID3D12Resource>& texture, ComPtr<ID3D12Resource>& textureUploadHeap, int& texId)
{
    if (!isRenderBegin)
    {
        TextureCreateInfo createInfo;
        createInfo.imageData = imageData;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.format = format;
        createInfo.pTexture = &texture;
        createInfo.pTextureUpload = &textureUploadHeap;
        createInfo.pTexId = &texId;

        m_textureCreateInfos.push_back(createInfo);
        return;
    }

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

    CD3DX12_CPU_DESCRIPTOR_HANDLE hSrvHeap(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), m_texCount + ReverseSRVCount, m_cbvUavSrvDescSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(texture.Get(), &srvDesc, hSrvHeap);

    texId = m_texCount;
    m_texCount++;

    texture->SetName(L"tex_buf");
    textureUploadHeap->SetName(L"tex_up_buf");
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
    if (!isRenderBegin)
    {
        VertexBufferCreateInfo createInfo;
        createInfo.vdata = vdata;
        createInfo.single = single;
        createInfo.length = length;
        createInfo.pVtxBuf = &vtxbuf;
        createInfo.pBufUploader = &bufuploader;
        createInfo.pVbv = &vbv;

        m_vertexBufferCreateInfos.push_back(createInfo);
        return;
    }

    const UINT vertexBufferSize = single * length;
    CreateDefaultBuffer(m_device, m_commandList, vdata, vertexBufferSize, vtxbuf, bufuploader);

    vbv.BufferLocation = vtxbuf->GetGPUVirtualAddress();
    vbv.StrideInBytes = single;
    vbv.SizeInBytes = vertexBufferSize;
    vtxbuf->SetName(L"vtx_buf");
    bufuploader->SetName(L"vtx_up_buf");;
}

void D12Renderer::CreateIndexBuffer(void* idata, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& indicebuf, ComPtr<ID3D12Resource>& bufuploader, D3D12_INDEX_BUFFER_VIEW& ibv)
{
    if (!isRenderBegin)
    {
        IndexBufferCreateInfo createInfo;
        createInfo.idata = idata;
        createInfo.single = single;
        createInfo.length = length;
        createInfo.pIndicebuf = &indicebuf;
        createInfo.pBufUploader = &bufuploader;
        createInfo.pIbv = &ibv;

        m_indexBufferCreateInfos.push_back(createInfo);
        return;
    }

    const UINT indiceBufferSize = single * length;
    CreateDefaultBuffer(m_device, m_commandList, idata, indiceBufferSize, indicebuf, bufuploader);

    ibv.BufferLocation = indicebuf->GetGPUVirtualAddress();
    ibv.Format = DXGI_FORMAT_R32_UINT;
    ibv.SizeInBytes = indiceBufferSize;
    indicebuf->SetName(L"indice_buf");
    bufuploader->SetName(L"indice_up_buf");;
}

void D12Renderer::CreateResources()
{
    // vertex buffers
    for (int i = 0; i < m_vertexBufferCreateInfos.size(); i++)
    {
        VertexBufferCreateInfo* createInfo = &m_vertexBufferCreateInfos[i];
        CreateVertexBuffer(createInfo->vdata, createInfo->single, createInfo->length, *createInfo->pVtxBuf, *createInfo->pBufUploader, *createInfo->pVbv);
    }
    m_vertexBufferCreateInfos.clear();

    // index buffers
    for (int i = 0; i < m_indexBufferCreateInfos.size(); i++)
    {
        IndexBufferCreateInfo* createInfo = &m_indexBufferCreateInfos[i];
        CreateIndexBuffer(createInfo->idata, createInfo->single, createInfo->length, *createInfo->pIndicebuf, *createInfo->pBufUploader, *createInfo->pIbv);
    }
    m_indexBufferCreateInfos.clear();

    // textures
    for (int i = 0; i < m_textureCreateInfos.size(); i++)
    {
        TextureCreateInfo* createInfo = &m_textureCreateInfos[i];
        CreateTexture(createInfo->imageData, createInfo->width, createInfo->height, createInfo->format, *createInfo->pTexture, *createInfo->pTextureUpload, *createInfo->pTexId);
    }
    m_textureCreateInfos.clear();
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

    constbuf->SetName(L"const_buf");
}

D3D12_RESOURCE_DESC D12Renderer::DescribeGPUBuffer(UINT bufSize)
{
    assert(bufSize != 0);

    D3D12_RESOURCE_DESC Desc = {};
    Desc.Alignment = 0;
    Desc.DepthOrArraySize = 1;
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    Desc.Format = DXGI_FORMAT_UNKNOWN;
    Desc.Height = 1;
    Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Desc.MipLevels = 1;
    Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;
    Desc.Width = (UINT64)bufSize;
    return Desc;
}

void D12Renderer::CreateUAVBuffer(void** data, uint32_t single, uint32_t length, ComPtr<ID3D12Resource>& uavbuf, CD3DX12_CPU_DESCRIPTOR_HANDLE& heapHandle)
{
    /// pure gpu buffer
    const UINT uavBufferSize = single * length;

    D3D12_HEAP_PROPERTIES heapProperty;
    heapProperty.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperty.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperty.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperty.CreationNodeMask = 1;
    heapProperty.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resDesc = DescribeGPUBuffer(uavBufferSize);

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
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

    uavbuf->SetName(L"uav_buf");
}

void D12Renderer::CreateDefaultBuffer(ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& cmdList, const void* initData, UINT64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& buffer, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    // Create the actual default buffer resource.
    D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(buffer.GetAddressOf())));

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
    D3D12_RESOURCE_BARRIER resBarrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &resBarrier);
    UpdateSubresources<1>(cmdList.Get(),
        buffer.Get(), uploadBuffer.Get(),
        0, 0, 1, &subResourceData);
    resBarrier = CD3DX12_RESOURCE_BARRIER::Transition(buffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &resBarrier);
}