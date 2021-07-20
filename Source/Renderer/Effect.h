#ifndef __EFFECT_H__
#define	__EFFECT_H__

#include "Renderer.h"

/*
	PostEffects (now only for dx renderer)
*/
class Effect
{
public:
	Effect( Renderer* pRenderer, const char* cType );
	virtual ~Effect();

	virtual void Process() = 0;

	const std::string& GetType() { return m_sType; }

protected:
	Renderer* m_pRenderer;
	std::string m_sType;
};

#endif // !__EFFECT_H__
