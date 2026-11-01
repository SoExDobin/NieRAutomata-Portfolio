#include "pch.h"
#include "State2B_Idle.h"
#include "Pl0000Body.h"
#include <Game.h>
#include <SpdLogger.h>

#include "Model.h"
#include "Pl0000.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"
#include "WP0070Body.h"
#include "WP0220Body.h"

State2B_Idle::State2B_Idle(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{tag, owner}
{
}

HRESULT State2B_Idle::Initialize()
{
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::RUN_STOP_R));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::RUN_STOP_L));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::SPRINT_STOP_R));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::JUMP_TO_STAND));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_TO_STAND_F));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_TO_STAND_B));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_TO_STAND_R));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_TO_STAND_L));

	return State2B::Initialize();
}

Bool State2B_Idle::StateEnterInvoke()
{
	auto pl0000 = m_Body.lock();

	switch (auto prevState = m_States.lock()->Get_CurPl0000State())
	{
	case Pl0000::PL0000_STATE::RUN:
		{
			if (m_Body.lock()->Get_ModelComponent()->Get_AnimationProgress() < 0.5f)
			{
				pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::RUN_STOP_R), 0.2f, false);
			}
			else
			{
				pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::RUN_STOP_L), 0.2f, false);
			}
			return true;
		}
	case Pl0000::PL0000_STATE::SPRINT:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::SPRINT_STOP_R), 0.2f, false);
		return true;
	case Pl0000::PL0000_STATE::JUMP: case Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_END : case Pl0000::PL0000_STATE::LIGHT_AIR_DOWN_END:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_TO_STAND), 0.2f, false);
		return true;
	case Pl0000::PL0000_STATE::EVADE:
		{
			// 현재 애니메이션이 블렌딩 중이라면 NextAnimIndex를 확인하여 정확한 DASH 애니메이션 도출
			auto model = m_Body.lock()->Get_ModelComponent();
			uint32 animIndex = model->Is_Blending() ? model->Get_NextAnimationIndex() : model->Get_AnimationIndex();

			switch (auto prevAnim = static_cast<Pl0000::PL0000_STATE>(animIndex))
			{
			case Pl0000::PL0000_STATE::DASH_F: case Pl0000::PL0000_STATE::STAND_TO_DASH_F:
				pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_TO_STAND_F), 0.2f, false);
				return true;
			case Pl0000::PL0000_STATE::DASH_B: case Pl0000::PL0000_STATE::STAND_TO_DASH_B:
				pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_TO_STAND_B), 0.2f, false);
				return true;
			case Pl0000::PL0000_STATE::DASH_R: case Pl0000::PL0000_STATE::STAND_TO_DASH_R: 
				pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_TO_STAND_R), 0.2f, false);
				return true;
			case Pl0000::PL0000_STATE::DASH_L: case Pl0000::PL0000_STATE::STAND_TO_DASH_L: 
				pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_TO_STAND_L), 0.2f, false);
				return true;
			case Pl0000::PL0000_STATE::EVADE_FRONT: case Pl0000::PL0000_STATE::EVADE_BACKWARD: 
			case Pl0000::PL0000_STATE::EVADE_RIGHT: case Pl0000::PL0000_STATE::EVADE_LEFT:
				pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::IDLE_STAND_TO_Neutral), 0.2f, false);
				return true;
			default:
				// [FIX] 만약 너무 빨리 취소되어 이전 애니메이션 (RUN_STOP 등)이 넘어온 경우 자연스럽게 IDLE로 전이
				pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::IDLE_STAND_TO_Neutral), 0.2f, false);
				return true;
			}
		}
	// The pod shot hands over after its recoil; Idle plays the return so any input can cut it short.
	case Pl0000::PL0000_STATE::POD_ATTACK:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::POD_LASER_RETURN), 0.2f, false);
		return true;
	case Pl0000::PL0000_STATE::ATTACK_GROUND: case Pl0000::PL0000_STATE::ATTACK_AIR: case Pl0000::PL0000_STATE::HIT:
	case Pl0000::PL0000_STATE::LEDGE:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::IDLE_STAND_TO_Neutral), 0.2f, false);
		return true;
	default:
		{
			if (m_InitializeState)
			{
				m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::IDLE_Neutral), 0.2f, false);

				m_Owner.lock()->Sheathe_LightWeapon();
				m_Owner.lock()->Sheathe_HeavyWeapon();
				m_InitializeState = false;

				return true;
			} 
			LOG_ERROR(L"[ENTER IDLE] : No Enter state PrevIndex {} ", ETOI(prevState));
			return false;
		}
	}

}

void State2B_Idle::Update(Float timeDelta)
{
	uint32 curIndex = m_Body.lock()->Get_CurrentAnimationIndex();

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
		if (m_States.lock()->Change_State(Pl0000::PL0000_STATE::JUMP))
			return;
	}
	if (m_Input.lock()->Is_WASD_DoubleClick())
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::EVADE);
		return;
	}
	if (m_Input.lock()->Is_WASD_Press())
	{
		if (m_States.lock()->Change_State(Pl0000::PL0000_STATE::RUN))
			return;
	}

	if (m_EnterAnim.contains(curIndex) && m_Body.lock()->Get_ModelComponent()->Is_AnimationFinished())
	{
		m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::IDLE_STAND_TO_Neutral), 0.5f, false);
	}
	// pl0000_1202 already ends standing, so the idle cycle takes over directly.
	else if (curIndex == ETOI(Pl0000::PL0000_STATE::POD_LASER_RETURN) && m_Body.lock()->Get_ModelComponent()->Is_AnimationFinished())
	{
		m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::IDLE_Neutral), 0.2f, true);
	}
	else if (curIndex == ETOI(Pl0000::PL0000_STATE::IDLE_STAND_TO_Neutral) && m_Body.lock()->Get_ModelComponent()->Is_AnimationFinished())
	{
		m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::IDLE_Neutral), 0.5f, true);
	}
}

void State2B_Idle::Late_Update(Float timeDelta)
{

}

void State2B_Idle::StateExitInvoke()
{

}

Shared<State2B_Idle> State2B_Idle::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_Idle>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_Idle");
		return nullptr;
	}

	return instance;
}
