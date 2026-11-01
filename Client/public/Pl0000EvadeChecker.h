#pragma once
#include "Pl0000Parts.h"

NS_BEGIN(Engine)
class SphereCollider;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL Pl0000EvadeChecker final : public PartObject
{
public:
	explicit Pl0000EvadeChecker();
	explicit Pl0000EvadeChecker(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000EvadeChecker(const Pl0000EvadeChecker& rhs);
	~Pl0000EvadeChecker() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	HRESULT Begin() override;

public:
	void Update(Float timeDelta) override;

private:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	Shared<SphereCollider> m_EvadeChecker{ nullptr };

public:
	static Shared<Pl0000EvadeChecker> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

};

NS_END
