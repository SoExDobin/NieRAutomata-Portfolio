#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

class CLIENT_DLL ParticleEffect abstract : public GameObject
{
public:
	typedef struct tagParticleEffectDesc : public GAMEOBJECT_DESC{} PARTICLE_EFFECT_DESC;

public:
	explicit ParticleEffect();
	explicit ParticleEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit ParticleEffect(const ParticleEffect& rhs);
	virtual ~ParticleEffect() override = default;
	
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Priority_Update(Float timeDelta) override {};
	void Update(Float timeDelta) override {};
	void Late_Update(Float timeDelta) override {};
	void Fixed_Update(Float fixedDelta) override {};

public:
	virtual Shared<GameObject> Clone(void* arg) override PURE;
	
};

NS_END
