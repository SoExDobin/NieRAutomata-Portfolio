#include "pch.h"
#include "State2B_PodAttack.h"

#include <Game.h>
#include <SpdLogger.h>

#include "Model.h"
#include "Pl0000.h"
#include "Pl0000Body.h"
#include "Pl0000Input.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"
#include "PodLaser.h"
#include "WP3000Body.h"

State2B_PodAttack::State2B_PodAttack(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{ tag, owner } {}

Bool State2B_PodAttack::StateEnterInvoke()
{
	m_Fired = false;
	m_SinceFire = 0.f;
	m_Laser =static_pointer_cast<PodLaser>(m_Owner.lock()->Find_Child(L"PodLaser"));
	Update_Facing();

	m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::POD_LASER_FIRE), 0.2f, false);
	m_Pod.lock()->Begin_Laser();

	return true;
}

void State2B_PodAttack::Update(Float timeDelta)
{
	Update_Facing();

	// The beam leaves once the pod has split its head open (wp3000_0041), along the pod's aim.
	if (!m_Fired && m_Pod.lock()->Is_LaserReady())
	{
		m_Fired = true;
		if (auto laser = m_Laser.lock())
			laser->Fire(m_Pod.lock()->Get_MuzzlePosition(), m_Pod.lock()->Get_AimDirection());
	}
	else if (m_Fired)
		m_SinceFire += timeDelta;

	// Once the beam's hit is over, input cuts the recoil short. Idle handles it in this same frame, so a click
	// (down for one frame only) is not lost. Leaving closes the pod's head (StateExitInvoke).
	auto states = m_States.lock();
	if (m_Fired && m_SinceFire >= n_CancelDelay && Has_ActionInput())
	{
		if (states->Change_State(Pl0000::PL0000_STATE::IDLE))
			states->Find_2BState(Pl0000::PL0000_STATE::IDLE)->Update(timeDelta);
		return;
	}

	// Without input, the rest of 1200 after the recoil is a still pose, so Idle takes over with the return (1202).
	if (m_Body.lock()->Get_ModelComponent()->Get_AnimationProgress() >= n_FireEnd)
		states->Change_State(Pl0000::PL0000_STATE::IDLE);
}

Bool State2B_PodAttack::Has_ActionInput() const
{
	// The inputs State2B_Idle turns into another state: attack, jump, evade and move.
	const auto input = m_Input.lock();
	return input->Is_MouseDown(DIMB::LBUTTON) || input->Is_MouseDown(DIMB::RBUTTON) ||
		input->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)) || input->Is_WASD_DoubleClick() ||
		input->Is_WASD_Down() || input->Is_WASD_Press();
}

void State2B_PodAttack::Late_Update(Float timeDelta)
{
}

void State2B_PodAttack::StateExitInvoke()
{
	m_Pod.lock()->End_Laser();
}

void State2B_PodAttack::Update_Facing()
{
	// The player keeps turning to face the way the pod aims.
	Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
	moveData.useRootMotionDir = true;
	moveData.canRotation = true;
	Vector3 look = m_Pod.lock()->Get_AimDirection();
	look.y = 0.f;
	if (look.LengthSquared() > 0.000001f)
	{
		look.Normalize();
		moveData.lookDirection = look;
	}
	m_Movement.lock()->Set_MovementData(moveData);
}

Shared<State2B_PodAttack> State2B_PodAttack::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_PodAttack>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_PodAttack");
		return nullptr;
	}

	return instance;
}
