#pragma once
#include "Pl0000Parts.h"

NS_BEGIN(Engine)
class SphereCollider;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL Pl0000MonsterChecker final : public PartObject
{
public:
	explicit Pl0000MonsterChecker();
	explicit Pl0000MonsterChecker(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000MonsterChecker(const Pl0000MonsterChecker& rhs);
	~Pl0000MonsterChecker() override = default;

public:
	// The nearest live monster ahead along look (flattened) within maxDistance, skipping exclude.
	Shared<GameObject> Get_ClosestTarget(Vector3 look, const Shared<GameObject>& exclude = nullptr, Float maxDistance = FLT_MAX);
	// The air attack's own pick keeps the range the checker had before G lock-on was widened.
	static constexpr Float AutoTargetRange = 12.5f;
	// Live monsters within radius on the ground plane and height above or below the player.
	vector<Shared<GameObject>> Get_MonstersAround(Float radius, Float height);

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	HRESULT Begin() override;
	void On_Destroy() override;

public:
	void Update(Float timeDelta) override;

private:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	Shared<SphereCollider> m_EvadeChecker{ nullptr };
	// G lock-on candidates: 1.5x the original 12.5 m.
	Float m_CheckDistance{ 18.75f };

	list<Weak<GameObject>> m_CheckedMonsters;

public:
	static Shared<Pl0000MonsterChecker> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

};

NS_END
