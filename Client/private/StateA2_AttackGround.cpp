#include "pch.h"
#include "StateA2_AttackGround.h"

#include <algorithm>
#include <SpdLogger.h>

#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"

StateA2_AttackGround::StateA2_AttackGround(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B_AttackGround{ tag, owner }
{
}

HRESULT StateA2_AttackGround::Initialize()
{
	using pl = Pl0000::PL0000_STATE;
	using wp = WP0220Body::WP0220_STATE;

	m_HeavyBody = {
		ETOI(pl::HEAVY_LIGHT_GROUND1),
		ETOI(pl::HEAVY_LIGHT_GROUND2),
		ETOI(pl::HEAVY_LIGHT_GROUND3),
		ETOI(pl::HEAVY_LIGHT_GROUND4),
	};
	m_HeavyWP = {
		ETOI(wp::LIGHT_GROUND1),
		ETOI(wp::LIGHT_GROUND2),
		ETOI(wp::LIGHT_GROUND3),
		ETOI(wp::LIGHT_GROUND4),
	};

	// The original pl0000_0200~0203 seq: the next hit is taken from flag 1, a move cuts the swing from flag 2.
	m_CanComboProgress.emplace(ETOI(pl::HEAVY_LIGHT_GROUND1), 47.f / 370.f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_LIGHT_GROUND1), 62.f / 370.f);
	m_CanComboProgress.emplace(ETOI(pl::HEAVY_LIGHT_GROUND2), 62.f / 333.f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_LIGHT_GROUND2), 73.f / 333.f);
	m_CanComboProgress.emplace(ETOI(pl::HEAVY_LIGHT_GROUND3), 77.f / 481.f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_LIGHT_GROUND3), 90.f / 481.f);
	m_CanComboProgress.emplace(ETOI(pl::HEAVY_LIGHT_GROUND4), 120.f / 490.f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_LIGHT_GROUND4), 120.f / 490.f);

	return State2B_AttackGround::Initialize();
}

void StateA2_AttackGround::Update(Float timeDelta)
{
	// Like the dodge, the jump leaves a swing at any frame; the air right click then spins up from it.
	const uint32 animIndex = m_Body.lock()->Get_CurrentAnimationIndex();
	if (m_Input.lock()->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)) && m_Movement.lock()->Is_Grounded() &&
		std::find(m_HeavyBody.begin(), m_HeavyBody.end(), animIndex) != m_HeavyBody.end() &&
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::JUMP))
		return;

	State2B_AttackGround::Update(timeDelta);
}

Shared<StateA2_AttackGround> StateA2_AttackGround::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<StateA2_AttackGround>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create StateA2_AttackGround");
		return nullptr;
	}

	return instance;
}
