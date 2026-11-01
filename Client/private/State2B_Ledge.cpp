#include "pch.h"
#include "State2B_Ledge.h"

#include <Game.h>
#include <SpdLogger.h>

#include "Pl0000.h"
#include "Pl0000Body.h"
#include "Pl0000Input.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"

namespace
{
	// pl0000_0080 (117 frames at 60/s): the root holds for 6 frames, rises 1.7 m by frame 24, then steps 1.53 m in.
	constexpr Float ClipRise = 1.7f;
	constexpr Float ClipSeconds = 117.f / 60.f;
	constexpr Float RiseEnd = 24.f / 117.f;			// progress where the feet are on the top
	constexpr Float RunOut = 44.f / 117.f;			// the step in is 1 m past the edge; any ground input may cut the rest short
	constexpr Float WallMaxNormalY = 0.4f;			// steeper than this counts as a wall
	constexpr Float FloorMinNormalY = 0.7f;			// flatter than this counts as a top to stand on
	constexpr Float TopProbeDepth = 0.3f;			// how far past the edge the top is looked for
	constexpr Float StandRoom = 1.7f;				// free height wanted above the top
	constexpr Float HandsOnWall = 0.4f;				// feet this far from the wall leave the hands on the edge
}

State2B_Ledge::State2B_Ledge(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{ tag, owner } {}

Bool State2B_Ledge::StateEnterInvoke()
{
	const Shared<Pl0000Movement> movement = m_Movement.lock();
	if (movement->Is_Grounded() || !Find_Ledge())
		return false;

	const Shared<Transform> transform = m_Owner.lock()->Get_Transform();
	transform->Set_Rotation(Quaternion::CreateFromYawPitchRoll(atan2f(m_Facing.x, m_Facing.z), 0.f, 0.f));
	transform->Update_WorldMatrix();
	movement->Reset_VerticalVelocity();

	Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
	moveData.isMove = true;
	moveData.canRotation = false;
	moveData.useRootMotionDir = true;
	moveData.rootMotionRise = m_Height / ClipRise;
	movement->Set_MovementData(moveData);

	m_IsOnTop = false;
	m_Body.lock()->Set_Animation(ETOI(Pl0000::PL0000_STATE::LEDGE_CLIMB), 0.1f, false);
	LOG_INFO(L"[Ledge] climb {:.2f} m (top {:.2f}, wall {:.2f} m ahead)", m_Height, m_TopY, m_Pull + HandsOnWall);
	return true;
}

void State2B_Ledge::Update(Float timeDelta)
{
	const Shared<Pl0000Movement> movement = m_Movement.lock();
	const Shared<Pl0000Body> body = m_Body.lock();
	const Float progress = body->Get_ModelComponent()->Get_AnimationProgress();

	if (!m_IsOnTop)
	{
		if (progress < RiseEnd)
		{
			// Draw the feet in during the rise so the hands land on the edge.
			movement->Add_Correction(m_Facing * (m_Pull * timeDelta / (RiseEnd * ClipSeconds)));
			return;
		}
		// The blend from the jump adds a little of its own root; settle on the top before the step in.
		m_IsOnTop = true;
		const Float y = m_Owner.lock()->Get_Transform()->Get_Position().y;
		if (m_TopY > y)
			movement->Add_Correction(Vector3{ 0.f, m_TopY - y, 0.f });
	}

	if (progress >= RunOut)
	{
		// The same inputs, in the same order, as State2B_Idle.
		const Shared<Pl0000Input> input = m_Input.lock();
		const Shared<Pl0000StateMachine> states = m_States.lock();
		if ((input->Is_MouseDown(DIMB::LBUTTON) || input->Is_MouseDown(DIMB::RBUTTON)) &&
			states->Change_State(Pl0000::PL0000_STATE::ATTACK_GROUND))
			return;
		if (input->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)) && states->Change_State(Pl0000::PL0000_STATE::JUMP))
			return;
		if (input->Is_WASD_DoubleClick() && states->Change_State(Pl0000::PL0000_STATE::EVADE))
			return;
		if (input->Is_WASD_Press() && states->Change_State(Pl0000::PL0000_STATE::RUN))
			return;
	}
	if (body->Is_AnimationFinished())
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
}

void State2B_Ledge::Late_Update(Float timeDelta)
{
}

void State2B_Ledge::StateExitInvoke()
{
	// The next state has already set its movement; only gravity is given back if it kept the climb's.
	const Shared<Pl0000Movement> movement = m_Movement.lock();
	Pl0000Movement::PL0000_MOVEMENT_DATA moveData = movement->Get_MovementData();
	if (moveData.rootMotionRise > 0.f)
	{
		moveData.rootMotionRise = 0.f;
		movement->Set_MovementData(moveData);
	}
}

Bool State2B_Ledge::Find_Ledge()
{
	const Shared<Pl0000> owner = m_Owner.lock();
	const Shared<Transform> transform = owner->Get_Transform();
	const Vector3 foot = transform->Get_Position();
	Vector3 forward = transform->Get_Look();
	forward.y = 0.f;
	if (forward.LengthSquared() < 1e-4f)
		return false;
	forward.Normalize();

	const Float minHeight = owner->Get_LedgeMinHeight();
	const Float maxHeight = owner->Get_LedgeMaxHeight();
	if (maxHeight <= minHeight)
		return false;

	// A wall ahead just under the lowest top that counts.
	const Vector3 low = foot + Vector3::UnitY * (minHeight - 0.1f);
	Vector3 wall{}, wallNormal{};
	if (!GAME_INSTANCE->Raycast_Physics(low, forward, owner->Get_LedgeReach(), wall, &wallNormal) ||
		fabsf(wallNormal.y) > WallMaxNormalY)
		return false;
	const Float wallDistance = Vector3::Distance(low, wall);

	// It ends below the highest top: clear air over it, which also keeps the next ray from starting inside it.
	const Float probeY = foot.y + maxHeight + 0.1f;
	Vector3 hit{}, normal{};
	if (GAME_INSTANCE->Raycast_Physics(Vector3{ foot.x, probeY, foot.z }, forward, wallDistance + TopProbeDepth, hit))
		return false;

	// The top, found from above just past the edge, in reach of the hands and flat enough to stand on.
	Vector3 above = wall + forward * TopProbeDepth;
	above.y = probeY;
	if (!GAME_INSTANCE->Raycast_Physics(above, -Vector3::UnitY, maxHeight - minHeight + 0.2f, hit, &normal) ||
		fabsf(normal.y) < FloorMinNormalY)
		return false;
	const Float height = hit.y - foot.y;
	if (height < minHeight || height > maxHeight)
		return false;

	// Room to stand up on it.
	Vector3 ceiling{};
	if (GAME_INSTANCE->Raycast_Physics(hit + Vector3::UnitY * 0.05f, Vector3::UnitY, StandRoom, ceiling))
		return false;

	// Square up to the wall: its normal turned to face the player, flattened.
	Vector3 facing = -wallNormal;
	facing.y = 0.f;
	if (facing.LengthSquared() < 1e-4f)
		facing = forward;
	facing.Normalize();
	if (facing.Dot(forward) < 0.f)
		facing = -facing;

	m_TopY = hit.y;
	m_Height = height;
	m_Facing = facing;
	m_Pull = max(wallDistance - HandsOnWall, 0.f);
	return true;
}

Shared<State2B_Ledge> State2B_Ledge::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_Ledge>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_Ledge");
		return nullptr;
	}

	return instance;
}
