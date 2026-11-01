#include "pch.h"
#include "State2B_AttackAir.h"

#include <algorithm>

#include "Monster.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"
#include "WP0070Body.h"
#include "WP0220Body.h"
#include "SpdLogger.h"

State2B_AttackAir::State2B_AttackAir(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{ tag, owner }
{
}

HRESULT State2B_AttackAir::Initialize()
{
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_ENTER));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::LIGHT_AIR_DOWN_ENTER));

	m_EndAnim.emplace(ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_END));
	m_EndAnim.emplace(ETOI(Pl0000::PL0000_STATE::LIGHT_AIR_DOWN_END));
	m_EndAnim.emplace(ETOI(Pl0000::PL0000_STATE::JUMP_TO_STAND));

	return State2B::Initialize();
}

Bool State2B_AttackAir::StateEnterInvoke()
{
	auto pl0000 = m_Owner.lock();
	auto prevState = m_States.lock()->Get_CurPl0000State();
	if (prevState != Pl0000::PL0000_STATE::JUMP && prevState != Pl0000::PL0000_STATE::ATTACK_GROUND)
	{
		LOG_ERROR(L"[ ENTER BY WRONG STATE, PREV State is not JUMP! it's {} ]", Helper::To_wString(magic_enum::enum_name(prevState)));
		return false;
	}

	if (false == HasTarget())
	{
		if (auto target = pl0000->Get_ClosestTarget())
			pl0000->Set_LockOnTarget(target);
	}

	m_Movement.lock()->Reset_RootMotionStop();

	if (m_Input.lock()->Is_MousePress(DIMB::LBUTTON))
		Begin_LightCombo();
	else if (m_Input.lock()->Is_MousePress(DIMB::RBUTTON) || prevState == Pl0000::PL0000_STATE::JUMP)
		Begin_HeavyDown();
	else
	{
		m_Mode = AIR_MODE::FALL;
		m_Movement.lock()->Set_Gravity(30.f);
		m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_HOLD), 0.2f, true);
		Pl0000Movement::PL0000_MOVEMENT_DATA movementData{};
		movementData.isJump = true;
		movementData.useRootMotionDir = false;
		m_Movement.lock()->Set_MovementData(movementData);
	}

	return true;
}

void State2B_AttackAir::Update(Float timeDelta)
{
	if (m_Mode == AIR_MODE::LIGHT)
	{
		Update_LightCombo(timeDelta);
		return;
	}

	auto pl0000 = m_Owner.lock();
	auto pl0000Body = m_Body.lock();
	auto animIndex = pl0000Body->Get_CurrentAnimationIndex();
	auto isFinished = pl0000Body->Is_AnimationFinished();

	if (m_Movement.lock()->Is_Grounded())
	{
		Update_Landing();
		return;
	}

	if (m_Mode == AIR_MODE::FALL)
	{
		// The combo is spent until landing; only the plunge and the pod glide are still allowed.
		if (m_Input.lock()->Is_MousePress(DIMB::RBUTTON))
		{
			Begin_HeavyDown();
			return;
		}
		if (Try_Glide())
			return;

		Pl0000Movement::PL0000_MOVEMENT_DATA movementData{};
		movementData.direction = Calculate_Direction();
		movementData.isMove = (movementData.direction.LengthSquared() > 0.f);
		movementData.isJump = true;
		movementData.useRootMotionDir = false;
		m_Movement.lock()->Set_MovementData(movementData);
		return;
	}

	Pl0000Movement::PL0000_MOVEMENT_DATA movementData{};
	movementData.isMove = false;
	movementData.isAttack = true;
	movementData.canRotation = false;
	// The plunge is driven by homing, not by the clip's root motion.
	movementData.useRootMotionDir = false;

	auto target = pl0000->Get_LockOnTarget();
	if (!target.expired() && !target.lock()->Is_Destroy() && target.lock()->Is_Active())
	{
		Vector3 pos = pl0000->Get_Transform()->Get_Position();
		Vector3 targetPos = target.lock()->Get_Transform()->Get_Position();
		Vector3 lookPos = targetPos;
		lookPos.y = pos.y;
		pl0000->Get_Transform()->LookAt(lookPos);

		Vector3 dirToTarget = targetPos - pos;
		dirToTarget.y = 0.f;
		Float distance = dirToTarget.Length();

		if (distance > 1.5f)
		{
			dirToTarget.Normalize();
			Float homingSpeed = 30.f;

			m_Movement.lock()->Add_Correction(dirToTarget * homingSpeed * timeDelta);
		}
	}
	else
	{
		Vector3 myLook = pl0000->Get_Transform()->Get_Look();
		myLook.y = 0.f;
		if (myLook.Length() > 0.001f) myLook.Normalize();
		Float dashSpeed = 8.f;
		m_Movement.lock()->Add_Correction(myLook * dashSpeed * timeDelta);
	}

	if (m_EnterAnim.contains(animIndex) && isFinished)
	{
		if (animIndex == ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_ENTER))
		{
			pl0000Body->Set_Animation(ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_HOLD), 0.1f, true);
			m_HeavyWeapon.lock()->Set_Animation(ETOI(WP0220Body::WP0220_STATE::HEAVY_AIR_DOWN_HOLD), 0.1f, true);
		}
		else
		{
			pl0000Body->Set_Animation(ETOI(Pl0000::PL0000_STATE::LIGHT_AIR_DOWN_HOLD), 0.1f, true);
			m_LightWeapon.lock()->Set_Animation(ETOI(WP0070Body::WP0070_STATE::LIGHT_AIR_DOWN_HOLD), 0.1f, true);
		}
	}

	movementData.isJump = true;
	m_Movement.lock()->Set_MovementData(movementData);
}

void State2B_AttackAir::Late_Update(Float timeDelta)
{

}

void State2B_AttackAir::StateExitInvoke()
{
	m_Owner.lock()->Sheathe_LightWeapon();
	m_Owner.lock()->Sheathe_HeavyWeapon();
	m_Movement.lock()->Reset_RootMotionStop();
	m_Movement.lock()->Set_Gravity(30.f);
}

void State2B_AttackAir::Begin_HeavyDown()
{
	auto pl0000 = m_Owner.lock();

	m_Mode = AIR_MODE::PLUNGE;
	m_Movement.lock()->Set_Gravity(80.f);
	m_Movement.lock()->Reset_VerticalVelocity();
	m_Movement.lock()->Add_Force(Vector3{ 0.f, n_PlungeLift, 0.f });

	m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_ENTER), 0.2f, false);
	pl0000->Sheathe_LightWeapon();
	pl0000->Draw_HeavyWeapon();
	m_HeavyWeapon.lock()->Set_Animation(ETOI(WP0220Body::WP0220_STATE::HEAVY_AIR_DOWN_ENTER), 0.2f, false);

	Slam_MonstersAround();
}

void State2B_AttackAir::Slam_MonstersAround()
{
	auto pl0000 = m_Owner.lock();
	for (const auto& target : pl0000->Get_MonstersAround(n_SlamRadius, n_SlamHeight))
		if (auto monster = Cast<Monster>(target))
			monster->Slam_Down(pl0000);
}

void State2B_AttackAir::Begin_LightDown()
{
	// The small sword's plunge: the same homing fall as the heavy one, the stab lands with a shockwave (WP0070 notify).
	m_Mode = AIR_MODE::PLUNGE;
	m_Movement.lock()->Set_Gravity(80.f);
	m_Movement.lock()->Reset_VerticalVelocity();
	m_Movement.lock()->Add_Force(Vector3{ 0.f, n_PlungeLift, 0.f });

	Face_ComboDirection();
	m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::LIGHT_AIR_DOWN_ENTER), 0.1f, false);
	m_LightWeapon.lock()->Set_Animation(ETOI(WP0070Body::WP0070_STATE::LIGHT_AIR_DOWN_ENTER), 0.1f, false);

	Slam_MonstersAround();
}

void State2B_AttackAir::Begin_LightCombo()
{
	auto pl0000 = m_Owner.lock();
	m_LightWeapon.lock()->Recall_ThrownWeapon();

	m_Mode = AIR_MODE::LIGHT;
	m_ComboStep = 0;
	// Hover while the combo lasts.
	m_Movement.lock()->Set_Gravity(0.f);
	m_Movement.lock()->Reset_VerticalVelocity();

	// Set once for the whole combo like the ground combo, so a hit's Reduce_RootMotion lasts until the state ends.
	Pl0000Movement::PL0000_MOVEMENT_DATA movementData{};
	movementData.isJump = true;
	movementData.isAttack = true;
	movementData.canRotation = false;
	movementData.useRootMotionDir = true;
	m_Movement.lock()->Set_MovementData(movementData);
	Face_ComboDirection();

	m_LastOrderedAnimIndex = LIGHT_AIR_BODY[m_ComboStep];
	m_Body.lock()->Set_Animation(m_LastOrderedAnimIndex, 0.1f, false);
	pl0000->Sheathe_HeavyWeapon();
	pl0000->Draw_LightWeapon();
	m_LightWeapon.lock()->Set_Animation(LIGHT_AIR_WP[m_ComboStep], 0.1f, false);
	++m_ComboStep;
}

void State2B_AttackAir::Update_LightCombo(Float timeDelta)
{
	auto pl0000 = m_Owner.lock();
	auto pl0000Body = m_Body.lock();
	auto input = m_Input.lock();
	auto movement = m_Movement.lock();

	if (input->Is_MousePress(DIMB::RBUTTON))
	{
		Begin_HeavyDown();
		return;
	}

	if (movement->Is_Grounded())
	{
		m_Mode = AIR_MODE::FALL;
		Update_Landing();
		return;
	}

	// Space breaks off the combo to hang from the pod.
	if (Try_Glide())
		return;

	// The clip's root motion carries the body forward; WASD adds air control and a lock-on target keeps the body facing it.
	const Vector3 direction = Calculate_Direction();
	if (direction.LengthSquared() > 0.f)
		movement->Add_Correction(direction * n_AirMoveSpeed * timeDelta);
	auto target = pl0000->Get_LockOnTarget();
	if (!target.expired() && !target.lock()->Is_Destroy() && target.lock()->Is_Active())
		Face_ComboDirection();

	const uint32 animIndex = pl0000Body->Get_CurrentAnimationIndex();
	if (animIndex != m_LastOrderedAnimIndex)
		return;

	const Float progress = pl0000Body->Get_AnimationProgress();
	if (m_ComboStep < 5 && progress >= n_AirComboProgress &&
		(input->Is_MousePress(DIMB::LBUTTON) || input->Is_MouseHold(DIMB::LBUTTON, 0.05f)))
	{
		Face_ComboDirection();
		m_LastOrderedAnimIndex = LIGHT_AIR_BODY[m_ComboStep];
		pl0000Body->Set_Animation(m_LastOrderedAnimIndex, 0.1f, false);
		m_LightWeapon.lock()->Set_Animation(LIGHT_AIR_WP[m_ComboStep], 0.1f, false);
		++m_ComboStep;
		return;
	}
	// A click after the fifth hit ends the combo with the stab down.
	if (m_ComboStep == 5 && progress >= n_AirComboProgress &&
		(input->Is_MousePress(DIMB::LBUTTON) || input->Is_MouseHold(DIMB::LBUTTON, 0.05f)))
	{
		Begin_LightDown();
		return;
	}

	if (pl0000Body->Is_AnimationFinished())
	{
		// Combo over: fall until landing, the combo cannot restart in this jump.
		m_Mode = AIR_MODE::FALL;
		movement->Set_Gravity(30.f);
		pl0000->Sheathe_LightWeapon();
		pl0000Body->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_HOLD), 0.2f, true);
	}
}

Bool State2B_AttackAir::Try_Glide()
{
	// A press, not a hold as in the jump: the air combo has no double jump for Space to mean instead. Letting go
	// drops into the jump's fall.
	return m_Input.lock()->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)) &&
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::POD_GLIDE);
}

void State2B_AttackAir::Update_Landing()
{
	auto pl0000Body = m_Body.lock();
	auto animIndex = pl0000Body->Get_CurrentAnimationIndex();
	auto isFinished = pl0000Body->Is_AnimationFinished();
	Float progress = pl0000Body->Get_AnimationProgress();

	Pl0000Movement::PL0000_MOVEMENT_DATA movementData{};
	movementData.isAttack = true;
	movementData.canRotation = false;
	movementData.isJump = false;
	movementData.useRootMotionDir = false;

	// 만약 땅에 먼저 닿으면 LIGHT, HEAVY END 애니메이션 되면서 끝
	if (false == m_EndAnim.contains(animIndex))
	{
		if (animIndex == ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_HOLD) || animIndex == ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_ENTER))
		{
			pl0000Body->Set_Animation(ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_DOWN_END), 0.05f, false);
			m_HeavyWeapon.lock()->Set_Animation(ETOI(WP0220Body::WP0220_STATE::HEAVY_AIR_DOWN_END), 0.05f, false);
		}
		else if (animIndex == ETOI(Pl0000::PL0000_STATE::LIGHT_AIR_DOWN_HOLD) || animIndex == ETOI(Pl0000::PL0000_STATE::LIGHT_AIR_DOWN_ENTER))
		{
			pl0000Body->Set_Animation(ETOI(Pl0000::PL0000_STATE::LIGHT_AIR_DOWN_END), 0.05f, false);
			m_LightWeapon.lock()->Set_Animation(ETOI(WP0070Body::WP0070_STATE::LIGHT_AIR_DOWN_END), 0.05f, false);
		}
		else
		{
			// Landed out of the light combo or the fall.
			m_Owner.lock()->Sheathe_LightWeapon();
			m_Movement.lock()->Set_Gravity(30.f);
			pl0000Body->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_TO_STAND), 0.1f, false);
		}

		m_Movement.lock()->Set_MovementData(movementData);
		return;
	}

	if (progress <= 0.1f)
	{
		m_Movement.lock()->Set_MovementData(movementData);
		return;
	}

	if (m_Input.lock()->Is_MousePress(DIMB::LBUTTON) || m_Input.lock()->Is_MousePress(DIMB::RBUTTON))
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::ATTACK_GROUND);
		return;
	}
	else if (m_Input.lock()->Is_WASD_SingleClickHold(0.5f) || m_Input.lock()->Is_WASD_Press())
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::RUN);
		return;
	}
	else if (isFinished)
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
		return;
	}

	m_Movement.lock()->Set_MovementData(movementData);
}

Shared<State2B_AttackAir> State2B_AttackAir::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_AttackAir>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : State2B_AttackAir");
		return nullptr;
	}

	return instance;
}
