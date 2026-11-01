#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

class CLIENT_DLL Projectile abstract : public GameObject
{
public:
	typedef struct tagProjectileDesc : public GAMEOBJECT_DESC
	{
		Vector3 initialPosition{};
		wstring targetLayer{};
		Vector3 direction{};
		Float speed{};
	} PROJECTILE_DESC;

public:
	explicit Projectile();
	explicit Projectile(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Projectile(const Projectile& rhs);
	virtual ~Projectile() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

protected:
	uint32 m_TargetLayerIndex{};
	Vector3 m_direction{};
	Float m_Speed{};

public:
	Shared<GameObject> Clone(void* arg = nullptr) override PURE;
	
};

NS_END
