#ifndef __LINEARIZE_DEPTH_H__
#define	__LINEARIZE_DEPTH_H__

#include "Effect.h"

class LinearizeDepth : public Effect
{
public:
	LinearizeDepth(Renderer* pRenderer);
	virtual ~LinearizeDepth();

	virtual void Process();

private:
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
	ComPtr<ID3D12Resource> m_linearDepth;
};

#endif // !__LINEARIZE_DEPTH_H__
