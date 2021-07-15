#include "Common/Utils.h"
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
}

LinearizeDepth::~LinearizeDepth()
{
	
}

void LinearizeDepth::Process(D3D12_GPU_DESCRIPTOR_HANDLE* inputs, int inputCount, D3D12_GPU_DESCRIPTOR_HANDLE* outputs, int outputCount)
{
	
}