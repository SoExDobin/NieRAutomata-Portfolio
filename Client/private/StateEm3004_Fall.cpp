#include "pch.h"
#include "StateEm3004_Fall.h"

#include <Game.h>
#include <SpdLogger.h>

#include "Em3004Body.h"
#include "EmMovement.h"

namespace
{
	constexpr Float FloorProbe = 200.f;
}

StateEm3004_Fall::StateEm3004_Fall(const wstring& tag, const Shared<Em3004>& owner)
	: StateEm3004{ tag, owner } {}

Bool StateEm3004_Fall::StateEnterInvoke()
{
	m_Dropping = true;
	m_HasFloor = false;
	m_FallSpeed = 0.f;
	m_HangTime = 0.f;
	Hold_Still(true);
	Play(ANIM::FALL, false, 0.f);
	m_Body.lock()->Set_AnimationRate(0.f);
	return true;
}

void StateEm3004_Fall::Update(Float timeDelta)
{
	if (!m_Dropping)
	{
		Hold_Still();
		if (Is_Finished(ANIM::FALL))
			Change_State(STATE::ATTACK);
		return;
	}

	Hold_Still(true);
	const Shared<Transform> transform = m_Owner.lock()->Get_Transform();
	Vector3 position = transform->Get_Position();
	if (!m_HasFloor)
	{
		Vector3 floor{};
		if (!GAME_INSTANCE->Raycast_Physics(position, -Vector3::UnitY, FloorProbe, floor))
		{
			LOG_WARN(L"Em3004 {}: no floor below the drop", m_Owner.lock()->Get_Name());
			Land();		// nothing below: the movement drops it as it would any spawn
			return;
		}
		m_FloorY = floor.y;
		m_HasFloor = true;
	}

	m_HangTime += timeDelta;
	if (m_HangTime < HangDuration)
		return;
	m_FallSpeed += DropGravity * timeDelta;
	position.y -= m_FallSpeed * timeDelta;
	if (position.y <= m_FloorY)
	{
		position.y = m_FloorY;
		Land();
	}
	transform->Set_Position(position);
	// The movement, flying, moves on from Get_Position, which reads the world matrix.
	transform->Update_WorldMatrix();
}

void StateEm3004_Fall::StateExitInvoke()
{
	// Cut short (it burst in the air): the next state's movement data, not flying, drops what is left.
	Land();
}

// The clip plays on from its first frame; from the next update the movement collides again (its controller starts here).
void StateEm3004_Fall::Land()
{
	m_Dropping = false;
	m_Body.lock()->Set_AnimationRate(1.f);
}

Shared<StateEm3004_Fall> StateEm3004_Fall::Create(const wstring& tag, const Shared<Em3004>& owner)
{
	auto instance = make_shared<StateEm3004_Fall>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3004_Fall");
		return nullptr;
	}

	return instance;
}
