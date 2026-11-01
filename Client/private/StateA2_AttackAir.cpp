#include "pch.h"
#include "StateA2_AttackAir.h"

#include "Pl0000Movement.h"
#include "WP0220Body.h"
#include "SpdLogger.h"

StateA2_AttackAir::StateA2_AttackAir(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B_AttackAir{ tag, owner }
{
}

Bool StateA2_AttackAir::StateEnterInvoke()
{
	m_IsRising = false;
	m_HasRisen = false;
	m_RiseCombo = false;
	m_RiseComboQueued = false;
	return State2B_AttackAir::StateEnterInvoke();
}

void StateA2_AttackAir::Update(Float timeDelta)
{
	auto input = m_Input.lock();
	const Bool clickLight = input->Is_MousePress(DIMB::LBUTTON) || input->Is_MouseHold(DIMB::LBUTTON, 0.05f);

	if (!m_IsRising)
	{
		if (m_RiseCombo && m_Mode == AIR_MODE::FALL && !m_Movement.lock()->Is_Grounded() && clickLight)
		{
			Begin_RiseCombo();
			return;
		}
		State2B_AttackAir::Update(timeDelta);
		return;
	}

	if (input->Is_MousePress(DIMB::LBUTTON))
		m_RiseComboQueued = true;

	auto target = m_Owner.lock()->Get_LockOnTarget();
	if (!target.expired() && !target.lock()->Is_Destroy() && target.lock()->Is_Active())
		Face_ComboDirection();

	auto pl0000Body = m_Body.lock();
	if (pl0000Body->Get_CurrentAnimationIndex() != ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_UP))
		return;
	if ((m_RiseComboQueued || clickLight) && pl0000Body->Get_AnimationProgress() >= n_RiseComboProgress)
		Begin_RiseCombo();
	else if (pl0000Body->Is_AnimationFinished())
		End_Rise();
}

void StateA2_AttackAir::StateExitInvoke()
{
	State2B_AttackAir::StateExitInvoke();
	m_IsRising = false;

	// The next state has already set its movement; only the clip's rise is taken back if it kept it.
	const Shared<Pl0000Movement> movement = m_Movement.lock();
	Pl0000Movement::PL0000_MOVEMENT_DATA moveData = movement->Get_MovementData();
	if (moveData.rootMotionRise > 0.f)
	{
		moveData.rootMotionRise = 0.f;
		movement->Set_MovementData(moveData);
	}
}

void StateA2_AttackAir::Begin_HeavyDown()
{
	if (m_HasRisen) return;
	m_HasRisen = true;
	m_IsRising = true;
	m_RiseCombo = true;
	m_RiseComboQueued = false;

	auto pl0000 = m_Owner.lock();

	// The clip carries the body up 3 m and a little forward; gravity waits meanwhile.
	Pl0000Movement::PL0000_MOVEMENT_DATA movementData{};
	movementData.isJump = true;
	movementData.isAttack = true;
	movementData.canRotation = false;
	movementData.useRootMotionDir = true;
	movementData.rootMotionRise = 1.f;
	m_Movement.lock()->Set_MovementData(movementData);
	Face_ComboDirection();

	m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::HEAVY_AIR_UP), 0.1f, false);
	pl0000->Sheathe_LightWeapon();
	pl0000->Draw_HeavyWeapon();
	m_HeavyWeapon.lock()->Set_Animation(ETOI(WP0220Body::WP0220_STATE::HEAVY_AIR_UP), 0.1f, false);
}

void StateA2_AttackAir::Begin_RiseCombo()
{
	// Hovers in 2B's air combo from where the spin left her; the light weapon takes over from the great sword.
	m_IsRising = false;
	m_RiseCombo = false;
	m_RiseComboQueued = false;
	Begin_LightCombo();
}

void StateA2_AttackAir::End_Rise()
{
	// Falls until landing as after the air combo; the rise does not come back in this jump, its combo still can.
	m_IsRising = false;
	m_Mode = AIR_MODE::FALL;
	m_Movement.lock()->Set_Gravity(30.f);

	Pl0000Movement::PL0000_MOVEMENT_DATA movementData{};
	movementData.isJump = true;
	movementData.useRootMotionDir = false;
	m_Movement.lock()->Set_MovementData(movementData);

	m_Owner.lock()->Sheathe_HeavyWeapon();
	m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::JUMP_HOLD), 0.2f, true);
}

Shared<StateA2_AttackAir> StateA2_AttackAir::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<StateA2_AttackAir>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateA2_AttackAir");
		return nullptr;
	}

	return instance;
}
