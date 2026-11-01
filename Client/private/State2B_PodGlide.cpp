#include "pch.h"
#include "State2B_PodGlide.h"

#include <SpdLogger.h>

#include "Pl0000.h"
#include "Pl0000Body.h"
#include "Pl0000Input.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"
#include "WP3000Body.h"

State2B_PodGlide::State2B_PodGlide(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{ tag, owner } {}

Bool State2B_PodGlide::StateEnterInvoke()
{
	// The jump keeps its momentum; gravity is given back on exit after Update has switched it off. It is saved there, not
	// here: the previous state's exit runs after this enter and may still set it (the air combo hovers with none).
	m_IsDescending = false;

	m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::POD_GRAB), 0.1f, false);
	m_Pod.lock()->Begin_Glide();

	return true;
}

void State2B_PodGlide::Update(Float timeDelta)
{
	auto movement = m_Movement.lock();

	// Jump owns both the fall after letting go and the landing, which keeps the move state the jump started from.
	if (movement->Is_Grounded() || m_Input.lock()->Is_NoneOrUp(UBYTE(DIKEYBOARD_SPACE)))
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::JUMP);
		return;
	}
	// Gliding into a ledge in reach climbs it, as the jump does.
	if (m_States.lock()->Change_State(Pl0000::PL0000_STATE::LEDGE))
		return;

	// Once the jump has spent its rise and the fall reaches glide speed, gravity is switched off for a constant slow fall.
	if (!m_IsDescending && movement->Get_VerticalVelocity() <= -n_GlideFallSpeed)
	{
		m_IsDescending = true;
		m_SavedGravity = movement->Get_Gravity();
		movement->Set_Gravity(0.f);
		movement->Reset_VerticalVelocity();
		movement->Add_Force(Vector3(0.f, -n_GlideFallSpeed, 0.f));
	}

	Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
	moveData.direction = Calculate_Direction();
	moveData.isJump = true;
	moveData.useRootMotionDir = false;
	movement->Set_MovementData(moveData);
	movement->Add_Correction(moveData.direction * n_GlideMoveSpeed * timeDelta);

	// Both clips have the same length, so seeking them together keeps the hand on the pod.
	auto body = m_Body.lock();
	if (body->Is_AnimationFinished())
	{
		body->Get_ModelComponent()->Set_AnimationProgress(n_GlideLoopStart);
		m_Pod.lock()->Get_ModelComponent()->Set_AnimationProgress(n_GlideLoopStart);
	}
}

void State2B_PodGlide::Late_Update(Float timeDelta)
{
}

void State2B_PodGlide::StateExitInvoke()
{
	if (m_IsDescending)
		m_Movement.lock()->Set_Gravity(m_SavedGravity);
	m_Pod.lock()->End_Glide();
}

Shared<State2B_PodGlide> State2B_PodGlide::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_PodGlide>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_PodGlide");
		return nullptr;
	}

	return instance;
}
