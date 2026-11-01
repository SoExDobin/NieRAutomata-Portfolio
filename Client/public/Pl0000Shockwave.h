#pragma once
#include "GameObject.h"
#include "Entity.h"

NS_BEGIN(Engine)
class SphereCollider;
class Shader;
class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

// One blast of 2B's heavy hits: hits every monster in its sphere once during the hit window. Where the floor is under it,
// it raises the ground dust and runs a refracting ring out over the floor.
class CLIENT_DLL Pl0000Shockwave final : public GameObject
{
public:
	typedef struct tagPlayerShockwaveDesc : public GAMEOBJECT_DESC
	{
		Vector3 position{};				// 생성 위치
		Float radius{ 0.f };			// 충격파 반경
		Entity::DAMAGE_INFO damageInfo; // 적용할 데미지 정보 (Player가 설정)
	} PLAYER_SHOCKWAVE_DESC;
public:
	explicit Pl0000Shockwave();
	explicit Pl0000Shockwave(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000Shockwave(const Pl0000Shockwave& rhs);
	~Pl0000Shockwave() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	void Submit_RenderGroup() override;
	HRESULT Render() override;

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Ready_Components(const PLAYER_SHOCKWAVE_DESC& desc);
	void Raise_GroundDust() const;

private:
	Float m_Age{};
	Bool m_HasGround{ false };
	Vector3 m_Ground{};
	Float m_RingRadius{};
	Shared<SphereCollider> m_ShockwaveCollider{ nullptr };
	Shared<Shader> m_Shader{ nullptr };
	Shared<VIBuffer_Particle_Point> m_Buffer{ nullptr };
	Entity::DAMAGE_INFO m_DamageInfo{};
	unordered_set<RuntimeObjectId> m_HitEntities;
	uint32 m_MonsterLayerIndex{};

	static constexpr Float HitWindow = 0.2f;
	static constexpr Float RingDuration = 0.35f;
	static constexpr Float RingReach = 1.3f;		// the ring's last radius over the blast radius
	static constexpr Float RingRefraction = 0.7f;
	static constexpr Float RingEmissive = 0.6f;
	// The blast splits the screen's colours (Game::Start_ScreenGlitch without bands), weaker than a hit's glitch.
	static constexpr Float ColourSplit = 0.3f;
	static constexpr Float ColourSplitTime = 0.25f;

public:
	static Shared<Pl0000Shockwave> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
