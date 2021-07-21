#include "Common/Utils.h"
#include "Camera.h"
#include "DRenderer.h"
#include "CameraVelocity.h"
#include "LinearizeDepth.h"

static LinearizeDepth* s_linearizeDepth;

CameraVelocity::CameraVelocity(Renderer* pRenderer)
    :Effect(pRenderer, "CameraVelocity")
{
    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    // b0 u0 t0
    CD3DX12_DESCRIPTOR_RANGE1 ParamRanges[3];
    ParamRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    ParamRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    ParamRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // computer shader always use all
    CD3DX12_ROOT_PARAMETER1 rootParameters[3];
    rootParameters[0].InitAsDescriptorTable(1, &ParamRanges[0], D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[1].InitAsDescriptorTable(1, &ParamRanges[1], D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[2].InitAsDescriptorTable(1, &ParamRanges[2], D3D12_SHADER_VISIBILITY_ALL);

    D12Renderer* dRenderer = (D12Renderer*)pRenderer;
    dRenderer->CreateRootSignature(rootSignatureFlags, rootParameters, _countof(rootParameters), m_rootSignature);

    std::vector<char> computeShader = Utils::readFile("Data/shader/CameraVelocityCS.cso");
    dRenderer->CreateComputePipeLineState(computeShader.data(), computeShader.size(), m_rootSignature, m_pipelineState);

    /// uav,srv for camera velocity buffer and one for const buffer
    dRenderer->CreateDescriptorHeap(dRenderer->GetFrameBufferCount() * 3, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, m_cbvSrvUavHeap);

    // camera velocity buffer
    CD3DX12_CPU_DESCRIPTOR_HANDLE hUavHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_CPU_DESCRIPTOR_HANDLE hSrvHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1, dRenderer->GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    for (int i = 0; i < dRenderer->GetFrameBufferCount(); i++)
    {
        dRenderer->CreateSrvUavTexArray(pRenderer->GetWinWidth(), pRenderer->GetWinHeight(), DXGI_FORMAT_R32_UINT, m_cameraVelocity[i], hUavHandle, hSrvHandle);
        NAME_D3D12_OBJECT_INDEXED(m_cameraVelocity, i);
        hUavHandle.Offset(dRenderer->GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3);
        hSrvHandle.Offset(dRenderer->GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3);
    }

    // const buffer view
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCbvHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 2, dRenderer->GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    for (int i = 0; i < dRenderer->GetFrameBufferCount(); i++)
    {
        dRenderer->CreateConstBuffer(&m_reprojectMtxConstBufferBegin[i], sizeof(glm::mat4x4), m_reprojectMtxConstBuffer[i], hCbvHandle);
        NAME_D3D12_OBJECT_INDEXED(m_reprojectMtxConstBuffer, i);
        hCbvHandle.Offset(dRenderer->GetDescSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3);
    }

    s_linearizeDepth = (LinearizeDepth*)dRenderer->GetEffect("LinearizeDepth");
    assert(s_linearizeDepth);

    isFirst = true;
}

CameraVelocity::~CameraVelocity()
{

}

void CameraVelocity::Process()
{
    Camera* pCamera = m_pRenderer->GetCamera();
    float farClip = pCamera->GetFarDistance();
    float nearClip = pCamera->GetNearDistance();
    uint32_t Width = m_pRenderer->GetWinWidth();
    uint32_t Height = m_pRenderer->GetWinHeight();

    float RcpHalfDimX = 2.0f / Width;
    float RcpHalfDimY = 2.0f / Height;
    const float RcpZMagic = nearClip / (farClip - nearClip);

    glm::mat4x4* reProjectMtx = pCamera->GetReProjectMatrix();
    glm::mat4x4 preMult = glm::mat4x4(
        glm::vec4(RcpHalfDimX, 0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, -RcpHalfDimY, 0.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, RcpZMagic, 0.0f),
        glm::vec4(-1.0f, 1.0f, -RcpZMagic, 1.0f)
    );
    glm::mat4x4 postMult = glm::mat4x4(
        glm::vec4(1.0f / RcpHalfDimX, 0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, -1.0f / RcpHalfDimY, 0.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        glm::vec4(1.0f / RcpHalfDimX, 1.0f / RcpHalfDimY, 0.0f, 1.0f));
    glm::mat4x4 CurToPrevXForm = postMult * (*reProjectMtx) * preMult;

    D12Renderer* dRenderer = (D12Renderer*)m_pRenderer;
    memcpy(m_reprojectMtxConstBufferBegin[dRenderer->GetFrameIndex()], &CurToPrevXForm, sizeof(glm::mat4x4));

    dRenderer->SetRootSignature(m_rootSignature);
    dRenderer->SetPipelineState(m_pipelineState);

    dRenderer->TransitionResource(s_linearizeDepth->GetLinearDepth(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    if (isFirst)
    {
        dRenderer->TransitionResource(m_cameraVelocity[dRenderer->GetFrameIndex()], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        isFirst = false;
    }

    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvUavHeap.Get(), s_linearizeDepth->GetSrvUavHeap().Get() };
    dRenderer->SetDescriptorHeaps(1, ppHeaps);
    dRenderer->SetComputeRootDescriptorTable(0, m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), dRenderer->GetFrameIndex() * 3 + 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    dRenderer->SetComputeRootDescriptorTable(1, m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), dRenderer->GetFrameIndex() * 3 + 0, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    dRenderer->SetDescriptorHeaps(1, ppHeaps + 1);
    dRenderer->SetComputeRootDescriptorTable(2, s_linearizeDepth->GetLinearDepthGpuHandle());
    dRenderer->Dispatch2D(dRenderer->GetWinWidth(), dRenderer->GetWinHeight());
}