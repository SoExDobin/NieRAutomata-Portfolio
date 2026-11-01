#include "pch.h"
#include "State2B_Jump.h"

#include <SpdLogger.h>

#include "Pl0000.h"
#include "Pl0000Body.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"

#include "Random_Helper.h"

State2B_Jump::State2B_Jump(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{tag, owner}
{
}

HRESULT State2B_Jump::Initialize()
{	
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::STAND_TO_JUMP));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::RUN_TO_JUMP));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::SPRINT_TO_JUMP));

	return State2B::Initialize();
}

Bool State2B_Jump::StateEnterInvoke()
{
	auto pl0000 = m_Body.lock();
	auto prevState = m_States.lock()->Get_CurPl0000State();

	// Letting go of the pod or ending an air dodge drops back into the fall: no new jump, the double jump and the air dodge
	// stay as they were (the double jump is still there after gliding from the first jump) and m_PrevMoveState is kept for
	// the landing.
	if (prevState == Pl0000::PL0000_STATE::POD_GLIDE ||
		(prevState == Pl0000::PL0000_STATE::EVADE && !m_Movement.lock()->Is_Grounded()))
	{
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_HOLD), 0.2f, true);
		return true;
	}

	m_CanDoubleJump = true;
	m_CanAirEvade = true;
	JumpScalar = 15.f;
	m_Movement.lock()->Add_Force(Vector3(0.f, JumpScalar, 0.f));

	switch (prevState)
	{
	// ATTACK_GROUND: A2 jumping out of her right-click combo (StateA2_AttackGround).
	case Pl0000::PL0000_STATE::IDLE: case Pl0000::PL0000_STATE::HIT: case Pl0000::PL0000_STATE::LEDGE:
	case Pl0000::PL0000_STATE::ATTACK_GROUND:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::STAND_TO_JUMP), 0.2f, false);
		m_PrevMoveState = Pl0000::PL0000_STATE::IDLE;
		return true;
	case Pl0000::PL0000_STATE::RUN:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::RUN_TO_JUMP), 0.2f, false);
		m_PrevMoveState = Pl0000::PL0000_STATE::RUN;
		return true;
	case Pl0000::PL0000_STATE::SPRINT: 
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::SPRINT_TO_JUMP), 0.2f, false);
		m_PrevMoveState = Pl0000::PL0000_STATE::SPRINT;
		return true;
	case Pl0000::PL0000_STATE::EVADE:
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::SPRINT_TO_JUMP), 0.2f, false);
		m_PrevMoveState = Pl0000::PL0000_STATE::EVADE;
		return true;
	default: 
		LOG_ERROR(L"[ENTER JUMP] : No Enter state PrevIndex {} ", Helper::To_wString(magic_enum::enum_name(prevState)));
		return false;
	}
}

void State2B_Jump::Update(Float timeDelta)
{
	Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
	moveData.direction = Calculate_Direction();
	moveData.isMove = (moveData.direction.LengthSquared() > 0.f);
	moveData.isJump = true;
	moveData.canRotation = (m_PrevMoveState != Pl0000::PL0000_STATE::EVADE);
	moveData.useRootMotionDir = false;
	m_Movement.lock()->Set_MovementData(moveData);
	m_Movement.lock()->Reduce_RootMotion(0.75f);

	auto pl0000 = m_Body.lock();
	uint32 curIndex = pl0000->Get_CurrentAnimationIndex();
	Bool isAnimFinished = pl0000->Get_ModelComponent()->Is_AnimationFinished();

	// 점프 상관 없이 무조건 땅 닿으면 끝
	if (m_Movement.lock()->Is_Grounded())
	{
		if (m_Input.lock()->Is_WASD_NoneOrUp())
		{
			m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
			return;
		}
		if (m_PrevMoveState == Pl0000::PL0000_STATE::SPRINT)
		{
			m_States.lock()->Change_State(Pl0000::PL0000_STATE::SPRINT);
			return;
		}
		if (m_Input.lock()->Is_WASD_SingleClickHold(0.5f) || m_Input.lock()->Is_WASD_Press())
		{
			m_States.lock()->Change_State(Pl0000::PL0000_STATE::RUN);
			return;
		}
	}

	// 손이 닿는 난간이 앞에 있으면 알아서 잡고 오른다(난간이 없으면 State2B_Ledge가 진입을 거절).
	if (!m_Movement.lock()->Is_Grounded() && m_States.lock()->Change_State(Pl0000::PL0000_STATE::LEDGE))
		return;

	// 점플 공격
	if (m_Input.lock()->Is_MousePress(DIMB::LBUTTON) ||
		m_Input.lock()->Is_MousePress(DIMB::RBUTTON))
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::ATTACK_AIR);
		return;
	}

	// 공중 회피: 착지할 때까지 한 번
	if (m_CanAirEvade && m_Input.lock()->Is_WASD_DoubleClick() &&
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::EVADE))
	{
		m_CanAirEvade = false;
		return;
	}

	// 더블 점프
	if ((m_CanDoubleJump && m_Input.lock()->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)))
		|| m_CanDoubleJump && m_Input.lock()->Is_KeyMultiClick(UBYTE(DIKEYBOARD_SPACE)))
	{
		// Frees rotation after an evade jump; a sprint jump keeps SPRINT so the landing returns to the sprint.
		if (m_PrevMoveState != Pl0000::PL0000_STATE::SPRINT)
			m_PrevMoveState = Pl0000::PL0000_STATE::IDLE;

		m_Movement.lock()->Add_Force(Vector3(0.f, JumpScalar, 0.f));
		if (false == m_Input.lock()->Is_NoneOrUp(UBYTE(DIKEYBOARD_S)))
		{
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DOUBLE_JUMP1), 0.2f, false);
		}
		else
		{
			uint32 doubleJumpIndex = Helper::Random_Int(ETOI(Pl0000::PL0000_STATE::DOUBLE_JUMP2), ETOI(Pl0000::PL0000_STATE::DOUBLE_JUMP4));
			pl0000->Set_Animation(doubleJumpIndex, 0.2f, false);
		}
		m_CanDoubleJump = false;    // 한 번만 가능하도록
	}

	// Pod glide: keeping Space held in the air grabs the pod, from the jump's own press as from the double jump's.
	if (m_Input.lock()->Is_KeyHold(UBYTE(DIKEYBOARD_SPACE), n_GlideHold))
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::POD_GLIDE);
		return;
	}

	if (m_EnterAnim.contains(curIndex) && isAnimFinished)
	{
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_ENTER), 0.2f, false);
	}
	else if (isAnimFinished && !m_Movement.lock()->Is_Grounded())
	{
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_HOLD), 0.2f, true);
	}
}

void State2B_Jump::Late_Update(Float timeDelta)
{
	
}

void State2B_Jump::StateExitInvoke()
{
	m_Movement.lock()->Reset_RootMotionStop();
}

Shared<State2B_Jump> State2B_Jump::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_Jump>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_Jump");
		return nullptr;
	}

	return instance;
}


