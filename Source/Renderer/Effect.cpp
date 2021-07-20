#include "Effect.h"

Effect::Effect(Renderer* pRenderer, const char* cType)
	:m_pRenderer(pRenderer)
	,m_sType(cType)
{
}

Effect::~Effect()
{
}