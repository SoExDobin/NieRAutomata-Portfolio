#pragma once
#include "Pl0000.h"
#include "State2B.h"
#include "WP0070Body.h"

NS_BEGIN(Client)

class Pl0000;

class CLIENT_DLL State2B_AttackAir : public State2B
{
public:
	explicit State2B_AttackAir(const wstring& tag, const Shared<Pl0000>& owner);
	~State2B_AttackAir() override = default;

public:
	// LIGHT: left-click air combo that hovers. PLUNGE: homing plunge, the right-click heavy one or the small sword's stab
	// that ends the air combo. FALL: combo finished, falls until landing.
	enum class AIR_MODE { LIGHT, PLUNGE, FALL };

protected:
	HRESULT Initialize() override;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void StateExitInvoke() override;

protected:
	// The right click in the air: 2B's homing plunge. A2 rises with a spin instead (StateA2_AttackAir).
	virtual void Begin_HeavyDown();
	void Begin_LightCombo();

private:
	void Begin_LightDown();
	// Both plunges take the machines hanging around her down with them.
	void Slam_MonstersAround();
	void Update_LightCombo(Float timeDelta);
	// Space in the air combo or the fall after it: hangs from the pod (State2B_PodGlide).
	Bool Try_Glide();
	void Update_Landing();

protected:
	AIR_MODE m_Mode{ AIR_MODE::PLUNGE };

private:
	uint32 m_ComboStep{};
	uint32 m_LastOrderedAnimIndex{};

	unordered_set<uint32> m_EnterAnim;
	unordered_set<uint32> m_EndAnim;

	const Float n_AirMoveSpeed{ 4.f };
	const Float n_AirComboProgress{ 0.35f };
	// Machines hanging this close to the plunge go down with it.
	const Float n_SlamRadius{ 3.f };
	const Float n_SlamHeight{ 4.f };
	// Both plunges hop up first (0.4 m at gravity 80) and stab down from there, so the slammed machine is under the sword.
	const Float n_PlungeLift{ 8.f };

	// Animation_Documentation_Automata.xlsx, Small Sword Light Air 1~5 (0130/0131/0130/0131/0132).
	const uint32 LIGHT_AIR_BODY[5] = {
		ETOI(Pl0000::PL0000_STATE::LIGHT_AIR1),
		ETOI(Pl0000::PL0000_STATE::LIGHT_AIR2),
		ETOI(Pl0000::PL0000_STATE::LIGHT_AIR3),
		ETOI(Pl0000::PL0000_STATE::LIGHT_AIR4),
		ETOI(Pl0000::PL0000_STATE::LIGHT_AIR5),
	};
	const uint32 LIGHT_AIR_WP[5] = {
		ETOI(WP0070Body::WP0070_STATE::LIGHT_AIR1),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_AIR2),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_AIR1),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_AIR2),
		ETOI(WP0070Body::WP0070_STATE::LIGHT_AIR3),
	};

public:
	static Shared<State2B_AttackAir> Create(const wstring& tag, const Shared<Pl0000>& owner);
};

NS_END