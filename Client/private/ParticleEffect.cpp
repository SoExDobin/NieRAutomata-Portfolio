#include "pch.h"
#include "ParticleEffect.h"

ParticleEffect::ParticleEffect() : GameObject{} {}
ParticleEffect::ParticleEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{device, context} {}
ParticleEffect::ParticleEffect(const ParticleEffect& rhs)
	: GameObject{rhs} {}
HRESULT ParticleEffect::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT ParticleEffect::Initialize(void* arg)
{
	return GameObject::Initialize(arg);
}