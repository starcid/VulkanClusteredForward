#include "Common/Utils.h"
#include "Camera.h"
#include "LinearizeDepth.h"

LinearizeDepth::LinearizeDepth(Renderer* pRenderer)
	:Effect(pRenderer)
{
    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    // b0 u0 t0
    CD3DX12_DESCRIPTOR_RANGE1 ParamRanges[2];
    ParamRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    ParamRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // computer shader always use all
    CD3DX12_ROOT_PARAMETER1 rootParameters[3];
    rootParameters[0].InitAsConstants(4, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[1].InitAsDescriptorTable(1, &ParamRanges[0], D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[2].InitAsDescriptorTable(1, &ParamRanges[1], D3D12_SHADER_VISIBILITY_ALL);

    D12Renderer* dRenderer = (D12Renderer*)pRenderer;
    dRenderer->CreateRootSignature(rootSignatureFlags, rootParameters, _countof(rootParameters), m_rootSignature);

    std::vector<char> computeShader = Utils::readFile("Data/shader/CameraVelocityCS.cso");
    dRenderer->CreateComputePipeLineState(computeShader.data(), computeShader.size(), m_rootSignature, m_pipelineState);

    /// uav,srv for linear buffer
    dRenderer->CreateDescriptorHeap(dRenderer->GetFrameBufferCount() * 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, m_srvUavHeap);

    // linear depth buffer
    CD3DX12_CPU_DESCRIPTOR_HANDLE hUavHandle(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_CPU_DESCRIPTOR_HANDLE hSrvHandle(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1, dRenderer->GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    for (int i = 0; i < dRenderer->GetFrameBufferCount(); i++)
    {
        dRenderer->CreateSrvUavTexArray(pRenderer->GetWinWidth(), pRenderer->GetWinHeight(), DXGI_FORMAT_R16_UNORM, m_linearDepth, hUavHandle, hSrvHandle);
        hUavHandle.Offset(dRenderer->GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2);
        hSrvHandle.Offset(dRenderer->GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2);
    }
}

LinearizeDepth::~LinearizeDepth()
{
	
}

void LinearizeDepth::Process()
{
    Camera* pCamera = m_pRenderer->GetCamera();
    float farClip = pCamera->GetFarDistance();
    float nearClip = pCamera->GetNearDistance();
    const float zMagic = (farClip - nearClip) / nearClip;

    D12Renderer* dRenderer = (D12Renderer*)m_pRenderer;
    dRenderer->SetRootSignature(m_rootSignature);
    dRenderer->SetPipelineState(m_pipelineState);

    dRenderer->TransitionResource(dRenderer->GetDepthStencil(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    dRenderer->TransitionResource(m_linearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    dRenderer->SetComputeConstants(0, zMagic);
    dRenderer->SetComputeRootDescriptorTable(1, m_srvUavHeap->GetGPUDescriptorHandleForHeapStart(), dRenderer->GetFrameIndex() * 2 + 0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    dRenderer->SetComputeRootDescriptorTable(2, dRenderer->GetDepthStencilGpuHandle());
    dRenderer->Dispatch2D(dRenderer->GetWinWidth(), dRenderer->GetWinHeight(), 16, 16);
}