#include "pch.h"
#include "State2B_Run.h"

#include <Game.h>
#include <SpdLogger.h>

#include "Pl0000.h"
#include "Pl0000Body.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"
#include "WP0070Body.h"

State2B_Run::State2B_Run(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{tag, owner}
{
}

HRESULT State2B_Run::Initialize()
{
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::JUMP_TO_RUN));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::WALK_TO_RUN));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::RUN_LIGHT_CYCLE));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::RUN_HEAVY_CYCLE));

	return State2B::Initialize();
}

Bool State2B_Run::StateEnterInvoke()
{
	auto pl0000 = m_Body.lock();

	switch (auto prevState = m_States.lock()->Get_CurPl0000State())
	{
	

	case Pl0000::PL0000_STATE::ATTACK_GROUND: case Pl0000::PL0000_STATE::EVADE: case Pl0000::PL0000_STATE::LEDGE:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::RUN_CYCLE), 0.2f, true);
		return true;
	case Pl0000::PL0000_STATE::JUMP: case Pl0000::PL0000_STATE::ATTACK_AIR:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_TO_RUN), 0.15f, false);
		return true;
	case Pl0000::PL0000_STATE::IDLE: case Pl0000::PL0000_STATE::HIT:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::WALK_TO_RUN), 0.2f, false);
		return true;
	default:
		LOG_ERROR(L"[ENTER RUN] : No Enter state PrevIndex {} ", Helper::To_wString(magic_enum::enum_name(prevState)));
		return false;
	}
}

void State2B_Run::Update(Float timeDelta)
{
	auto pl0000 = m_Body.lock();
	uint32 curIndex = m_Body.lock()->Get_CurrentAnimationIndex();
	Bool isAnimFinished = m_Body.lock()->Get_ModelComponent()->Is_AnimationFinished();

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

	if (m_Input.lock()->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)))
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::JUMP);
		return;
	}

	if (m_Input.lock()->Is_WASD_DoubleClick())
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::EVADE);
		return;
	}

	if (m_EnterAnim.contains(curIndex) && isAnimFinished)
	{
		m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::RUN_CYCLE), 0.2f, true);
		return;
	}

	if (m_Input.lock()->Is_WASD_None())
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
}

void State2B_Run::Late_Update(Float timeDelta)
{

}

void State2B_Run::StateExitInvoke()
{
	
}

Shared<State2B_Run> State2B_Run::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_Run>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_Run");
		return nullptr;
	}

	return instance;
}
