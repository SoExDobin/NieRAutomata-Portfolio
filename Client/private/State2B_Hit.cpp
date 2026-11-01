#include "pch.h"
#include "State2B_Hit.h"

#include <Random_Helper.h>
#include <SpdLogger.h>

#include "Pl0000.h"
#include "Pl0000Body.h"
#include "Pl0000Input.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"

State2B_Hit::State2B_Hit(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{tag, owner} {}

HRESULT State2B_Hit::Initialize()
{
	m_HitAnim.emplace(ETOI(Pl0000::PL0000_STATE::HIT1));
	m_HitAnim.emplace(ETOI(Pl0000::PL0000_STATE::HIT2));
	m_HitAnim.emplace(ETOI(Pl0000::PL0000_STATE::HIT3));
	m_HitAnim.emplace(ETOI(Pl0000::PL0000_STATE::HIT4));

	return State2B::Initialize();
}

Bool State2B_Hit::StateEnterInvoke()
{
	uint32 animIndex = Helper::Random_Int(ETOI(Pl0000::PL0000_STATE::HIT1), ETOI(Pl0000::PL0000_STATE::HIT4));
	m_Body.lock()->Set_Animation(animIndex, 0.1f, false);

	// Drop the previous state's move direction; the hit clip's own root motion pushes the player.
	Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
	moveData.canRotation = false;
	moveData.useRootMotionDir = true;
	m_Movement.lock()->Set_MovementData(moveData);

	return true;
}

void State2B_Hit::Update(Float timeDelta)
{
	auto pl0000 = m_Body.lock();
	auto input = m_Input.lock();
	auto states = m_States.lock();

	if (m_HitAnim.contains(pl0000->Get_CurrentAnimationIndex()) && pl0000->Get_AnimationProgress() >= 0.25f)
	{
		if (input->Is_MouseDown(DIMB::LBUTTON) || input->Is_MouseDown(DIMB::RBUTTON))
		{
			if (states->Change_State(Pl0000::PL0000_STATE::ATTACK_GROUND))
				return;
		}
		if (input->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)))
		{
			if (states->Change_State(Pl0000::PL0000_STATE::JUMP))
				return;
		}
		if (input->Is_WASD_DoubleClick())
		{
			states->Change_State(Pl0000::PL0000_STATE::EVADE);
			return;
		}
		if (input->Is_WASD_Press())
		{
			if (states->Change_State(Pl0000::PL0000_STATE::RUN))
				return;
		}
	}

	// Checked apart from the input window above, which stays open once the clip passes 25%.
	if (pl0000->Is_AnimationFinished())
		states->Change_State(Pl0000::PL0000_STATE::IDLE);
}

void State2B_Hit::Late_Update(Float timeDelta)
{
}

void State2B_Hit::StateExitInvoke()
{
}

Shared<State2B_Hit> State2B_Hit::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_Hit>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_Hit");
		return nullptr;
	}

	return instance;
}
