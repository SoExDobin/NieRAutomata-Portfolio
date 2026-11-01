#include "pch.h"
#include "Pl0000.h"
#include "Pl0000Parts.h"

#include <Model.h>


Pl0000Parts::Pl0000Parts() : PartObject{} {}

Pl0000Parts::Pl0000Parts(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{device, context} {}

Pl0000Parts::Pl0000Parts(const Pl0000Parts& rhs)
	: PartObject{rhs} {}

HRESULT Pl0000Parts::Initialize_Prototype()
{
	return PartObject::Initialize_Prototype();
}

HRESULT Pl0000Parts::Initialize(void* arg)
{
	return PartObject::Initialize(arg);
}

void Pl0000Parts::Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop)
{
	m_Model->Set_Animation(animIndex, blendDuration);
	m_Model->Set_AnimLoop(isLoop);
}

