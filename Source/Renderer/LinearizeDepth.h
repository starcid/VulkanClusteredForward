#ifndef __LINEARIZE_DEPTH_H__
#define	__LINEARIZE_DEPTH_H__

#include "Effect.h"

class LinearizeDepth : public Effect
{
public:
	LinearizeDepth(Renderer* pRenderer);
	virtual ~LinearizeDepth();

	void Process(D3D12_GPU_DESCRIPTOR_HANDLE* inputs, int inputCount, D3D12_GPU_DESCRIPTOR_HANDLE* outputs, int outputCount);

private:
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;
};

#endif // !__LINEARIZE_DEPTH_H__
