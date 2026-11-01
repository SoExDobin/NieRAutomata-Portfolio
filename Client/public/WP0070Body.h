#pragma once
#include "Pl0000Parts.h"

NS_BEGIN(Engine)
class OBBCollider;
class SphereCollider;
class SoftBody;
NS_END


NS_BEGIN(Client)

class CLIENT_DLL WP0070Body final : public Pl0000Parts
{
public:
	explicit WP0070Body();
	explicit WP0070Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit WP0070Body(const WP0070Body& rhs);
	~WP0070Body() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Enable() override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

public:
	TRANSFORM_FRAME Get_ModelTransform() const { return m_Model->Get_RootTransformVelocity(m_RootBoneIndex); }
	void Set_Sheathing();
	void DrawWP0070();
	Bool Is_Sheathing() const { return m_IsSheathing; }
	void Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop) override;
	void Begin_LaunchAttack();
	void End_LaunchAttack();
	void Recall_ThrownWeapon();
	Bool Is_Thrown() const { return m_ThrowPhase != THROW_PHASE::ATTACHED; }

public:
	Bool Is_DetachedTransform() const { return Is_Thrown(); }
	void Set_DetachedTransform(Bool isDetached) { if (!isDetached) Recall_ThrownWeapon(); }

private:
	void Active_LightWeapon();
	void DeActive_LightWeapon();
	void Impact_Shockwave(const Vector3& offset);
	void Begin_Throw();
	// The clips m_LaunchCollider strikes in: the launch (LIGHT_GROUND6) and the air combo's swings.
	Bool Is_SphereSwing(uint32 animIndex) const;

private:
	HRESULT Bind_ShaderResources();
	HRESULT Ready_Components();
	HRESULT Ready_AnimationNotify();

private:
	int32 m_RootBoneIndex{};
	int32 m_WeaponBoneIndex{};
	Bool m_IsSheathing{ true };
	// Set while sheathed: the model stops updating then, so its pose is stale and the next clip must not blend out of it.
	Bool m_SnapNextAnimation{ true };
	Shared<OBBCollider> m_AttackCollider{ nullptr };
	Shared<SoftBody> m_SoftBody{ nullptr };
	Vector3 m_PrevAttackPivot{};
	Shared<SphereCollider> m_LaunchCollider{ nullptr };
	unordered_set<RuntimeObjectId> m_HitEntities;
	Bool m_PlayerLaunched{ false };

	enum class THROW_PHASE { ATTACHED, FLYING, STUCK, RETURNING };
	THROW_PHASE m_ThrowPhase{ THROW_PHASE::ATTACHED };
	Vector3 m_ThrowDirection{};
	// The thrown blade's world matrix (bone0's): it leaves the player and the clip, which goes on playing, alone.
	Matrix m_ThrownBlade{};
	Float m_ThrowElapsed{};
	Bool m_ReturnToSheath{ false };


public:
	static Shared<WP0070Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

public:
	enum class WP0070_STATE {
		SHEATHE_LIGHT = 0,
		SHEATHE_HEAVY = 1,

		LIGHT_GROUND1		= 4,
		LIGHT_GROUND2		= 5,
		LIGHT_GROUND3		= 6,
		LIGHT_GROUND4		= 7,
		LIGHT_GROUND5		= 8,
		LIGHT_GROUND6		= 9,
		LIGHT_GROUND7		= 10,

		LIGHT_GROUND_RUN	= 32,
		LIGHT_GROUND_HOLD	= 50,
		
		LIGHT_AIR1			= 20,
		LIGHT_AIR2			= 21,
		LIGHT_AIR3			= 22,

		LIGHT_AIR_DOWN_ENTER	= 34,
		LIGHT_AIR_DOWN_HOLD		= 35,
		LIGHT_AIR_DOWN_END		= 36,

		LIGHT_COMBO			= 45
	};
};

NS_END

/*
 wp0070_0000.mot > 0
wp0070_0001.mot > 1
wp0070_005a.mot > 2
wp0070_005b.mot > 3
wp0070_0100.mot > 4
wp0070_0101.mot > 5
wp0070_0102.mot > 6
wp0070_0103.mot > 7
wp0070_0104.mot > 8
wp0070_0105.mot > 9
wp0070_0106.mot > 10 
wp0070_0107.mot > 11
wp0070_0108.mot > 12
wp0070_0110.mot > 13
wp0070_0111.mot > 14
wp0070_0113.mot > 15
wp0070_0114.mot > 16
wp0070_0116.mot > 17
wp0070_0117.mot > 18
wp0070_0119.mot > 19
wp0070_0130.mot > 20 (공중 1,3타)
wp0070_0131.mot > 21 (공중 2,4타)
wp0070_0132.mot > 22 (공중 5타)
wp0070_0133.mot > 23
wp0070_0134.mot > 24
wp0070_0135.mot > 25
wp0070_0136.mot > 26
wp0070_0150.mot > 27
wp0070_0155.mot > 28
wp0070_0156.mot > 29
wp0070_0157.mot > 30
wp0070_0160.mot > 31
wp0070_0180.mot > 32
wp0070_0188.mot > 33
wp0070_018d.mot > 34 // dir 
wp0070_018e.mot > 35
wp0070_018f.mot > 36
wp0070_01e0.mot > 37
wp0070_01e1.mot > 38
wp0070_01e8.mot > 39
wp0070_01e9.mot > 40
wp0070_01f0.mot > 41
wp0070_01f5.mot > 42
wp0070_01f6.mot > 43
wp0070_01f7.mot > 44
wp0070_0600.mot > 45
wp0070_0640.mot > 46
wp0070_0643.mot > 47
wp0070_0644.mot > 48
wp0070_064c.mot > 49
wp0070_0655.mot > 50
wp0070_0660.mot > 51 (지상 Hold)
wp0070_0900.mot > 52
wp0070_0901.mot > 53
wp0070_0902.mot > 54
wp0070_0903.mot > 55
wp0070_0904.mot > 56
wp0070_0905.mot > 57
wp0070_0906.mot > 58
wp0070_0907.mot > 59
wp0070_0908.mot > 60
wp0070_0909.mot > 61
wp0070_090a.mot > 62
wp0070_090b.mot > 63
wp0070_c001.mot > 64
wp0070_c002.mot > 65
wp0070_e089.mot > 66
wp0070_e0e5.mot > 67
wp0070_e0e6.mot > 68
wp0070_e0e7.mot > 69
wp0070_e0e8.mot > 70
 */
