#pragma once
#include "Pl0000.h"
#include "WP0070Body.h"
#include "WP0220Body.h"
#include "State2B.h"

NS_BEGIN(Client)
	class Pl0000;

class CLIENT_DLL State2B_AttackGround : public State2B
{
public:
	enum class COMBO_TYPE { LIGHT, HEAVY, LIGHT_HEAVY, END};
public:
	explicit State2B_AttackGround(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_AttackGround() override = default;

protected:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Execute_Attack();
	void Set_AnimationExitProgress();

private:
	uint32 m_ComboStep{};
	COMBO_TYPE m_PrevComboType{};
	Float m_ComboDelta{};
	uint32 m_LastOrderedAnimIndex{ };

protected:
	unordered_map<uint32, Float> m_CanComboProgress;
	unordered_map<uint32, Float> m_CanExitProgress;

private:
	unordered_set<uint32> m_EnterAnim;

	// The left button has been let go since the hold attack started; until then its press is the hold's, not a new click.
	Bool m_HoldReleased{ true };
	// pl0000_0660 (331 frames) cuts its last hit at frame 60 and settles its feet by 176; the rest is a still pose.
	const Float n_HoldEnd = { 180.f / 331.f };

	Bool m_IsHeavyCharge{};
	Bool m_IsChargeEnd{};
	Float m_HeavyChargeDelta{};
	enum class AIR_WEAPON { NONE, LIGHT, HEAVY };
	AIR_WEAPON m_AirWeapon{ AIR_WEAPON::NONE };

	const uint32 LIGHT_BODY[7] = {
		ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND1),
		ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND2),
		ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND3),
		ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND4),
		ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND5),
		ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND6),
		ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND7),
	};
	const uint32 LIGHT_WP[7] = {
		ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND1),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND2),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND3),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND4),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND5),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND6),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND7),
	};

protected:
	// The right-click combo, body clip and great sword clip per hit. 2B's three heavy hits; A2 replaces them.
	vector<uint32> m_HeavyBody{
		ETOI(Pl0000::PL0000_STATE::HEAVY_GROUND1),
		ETOI(Pl0000::PL0000_STATE::HEAVY_GROUND2),
		ETOI(Pl0000::PL0000_STATE::HEAVY_GROUND3),
	};
	vector<uint32> m_HeavyWP{
		ETOI(WP0220Body::WP0220_STATE::HEAVY_GROUND1),
		ETOI(WP0220Body::WP0220_STATE::HEAVY_GROUND2),
		ETOI(WP0220Body::WP0220_STATE::HEAVY_GROUND3),
	};


public:
	static Shared<State2B_AttackGround> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END

