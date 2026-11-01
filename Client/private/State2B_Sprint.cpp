#include "pch.h"
#include "State2B_Sprint.h"

#include <SpdLogger.h>

#include "Pl0000.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"

State2B_Sprint::State2B_Sprint(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{tag, owner}
{
}

HRESULT State2B_Sprint::Initialize()
{
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_F));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_B));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_R));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_L));

	m_ToSprint.emplace(ETOI(Pl0000::PL0000_STATE::JUMP_TO_SPRINT));
	m_ToSprint.emplace(ETOI(Pl0000::PL0000_STATE::DASH_F_TO_SPRINT));
	m_ToSprint.emplace(ETOI(Pl0000::PL0000_STATE::DASH_B_TO_SPRINT));
	m_ToSprint.emplace(ETOI(Pl0000::PL0000_STATE::DASH_R_TO_SPRINT));
	m_ToSprint.emplace(ETOI(Pl0000::PL0000_STATE::DASH_L_TO_SPRINT));

	return State2B::Initialize();
}

Bool State2B_Sprint::StateEnterInvoke()
{
	auto pl0000 = m_Body.lock();
	auto input = m_Input.lock();
	auto stateIndex = m_States.lock()->Get_CurPl0000State();

	m_LockedDirection = Calculate_Direction();
	if (stateIndex == Pl0000::PL0000_STATE::JUMP)
	{
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_TO_SPRINT), 0.2f, false);
		return true;
	}
	else
	{
		switch (auto animIndex = static_cast<Pl0000::PL0000_STATE>(pl0000->Get_ModelComponent()->Get_AnimationIndex()))
		{
		case Pl0000::PL0000_STATE::DASH_F: case Pl0000::PL0000_STATE::EVADE_FRONT:
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_F_TO_SPRINT), 0.25f, false);
			return true;
		case Pl0000::PL0000_STATE::DASH_B: case Pl0000::PL0000_STATE::EVADE_BACKWARD:
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_B_TO_SPRINT), 0.25f, false);
			return true;
		case Pl0000::PL0000_STATE::DASH_R: case Pl0000::PL0000_STATE::EVADE_RIGHT:
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_R_TO_SPRINT), 0.25f, false);
			return true;
		case Pl0000::PL0000_STATE::DASH_L: case Pl0000::PL0000_STATE::EVADE_LEFT:
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_L_TO_SPRINT), 0.25f, false);
			return true;

		default:
			LOG_ERROR(L"[ENTER SPRINT] : No Enter Animation state PrevAnimIndex {} ",
				Helper::To_wString(magic_enum::enum_name(static_cast<Pl0000::PL0000_STATE>(animIndex))));
			return false;
		}
	}
}

void State2B_Sprint::Update(Float timeDelta)
{
	auto pl0000 = m_Body.lock();
	auto input = m_Input.lock();
	uint32 curIndex = pl0000->Get_ModelComponent()->Get_AnimationIndex();
	Bool isAnimFinished = pl0000->Get_ModelComponent()->Is_AnimationFinished();

	Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
	moveData.direction = Calculate_Direction();
	moveData.isMove = (moveData.direction.LengthSquared() > 0.f);
	moveData.canRotation = true;

	m_Movement.lock()->Set_MovementData(moveData);

	if (m_Input.lock()->Is_MouseDown(DIMB::LBUTTON) || m_Input.lock()->Is_MouseDown(DIMB::RBUTTON))
	{
		if (m_States.lock()->Change_State(Pl0000::PL0000_STATE::ATTACK_GROUND))
			return;
	}

	if (input->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)))
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::JUMP);
		return;
	}

	if (m_ToSprint.contains(curIndex) && isAnimFinished)
	{
		m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::SPRINT_CYCLE), 0.2f, true);
		return;
	}
	
	if (input->Is_WASD_NoneOrUp())
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
	}
}

void State2B_Sprint::Late_Update(Float timeDelta)
{
	
}

void State2B_Sprint::StateExitInvoke()
{
	State2B::StateExitInvoke();
}

Shared<State2B_Sprint> State2B_Sprint::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_Sprint>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_Sprint");
		return nullptr;
	}

	return instance;
}

