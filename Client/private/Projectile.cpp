#include "pch.h"
#include "Projectile.h"

#include <Game.h>

#include "SpdLogger.h"

Projectile::Projectile() : GameObject{} {}
Projectile::Projectile(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{device, context} {}
Projectile::Projectile(const Projectile& rhs)
	: GameObject{rhs} {}

HRESULT Projectile::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT Projectile::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize Projectile");
		return E_FAIL;
	}

	if (PROJECTILE_DESC* desc = static_cast<PROJECTILE_DESC*>(arg))
	{
		m_Transform->Set_Position(desc->initialPosition);
		m_TargetLayerIndex = ETOI(GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(desc->targetLayer));
		m_direction = desc->direction;
		m_Speed = desc->speed;
	}
	else
	{
		LOG_ERROR(L"Failed to Initialize Projectile By Desc");
		return E_FAIL;
	}

	return S_OK;
}
