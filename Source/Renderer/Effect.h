#ifndef __EFFECT_H__
#define	__EFFECT_H__

#include "DRenderer.h"

/*
	PostEffects (now only for dx renderer)
*/
class Effect
{
public:
	Effect( Renderer* pRenderer );
	virtual ~Effect();

	virtual void Process(D3D12_GPU_DESCRIPTOR_HANDLE* inputs, int inputCount, D3D12_GPU_DESCRIPTOR_HANDLE* outputs, int outputCount) = 0;

protected:
	Renderer* m_pRenderer;
};

#endif // !__EFFECT_H__
