#include "pch.h"
#include "StateEm8000_Idle.h"

#include "Random_Helper.h"

StateEm8000_Idle::StateEm8000_Idle(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Idle::StateEnterInvoke()
{
	if (m_Owner.lock()->Is_Dead()) return false;
	Play(ANIM::IDLE, true, 0.3f);
	m_Delay = static_cast<Float>(Helper::Random_Double(MinDelay, MaxDelay));
	return true;
}

void StateEm8000_Idle::Update(Float timeDelta)
{
	if (!Has_Target())
	{
		Hold_Still();
		return;
	}
	Hold_FacingTarget();
	m_Delay -= timeDelta;
	if (m_Delay > 0.f) return;
	Choose_Next();
}

void StateEm8000_Idle::Choose_Next()
{
	// HP is down to the rage share: it turns into the ball for good.
	if (m_Owner.lock()->Is_BallDue())
	{
		Change_State(STATE::BALL);
		return;
	}

	// The first round shows every attack once wherever the player stands (MOVE is not one): the laser opens, the rest
	// follow in PATTERN order; then they are picked by range and bearing.
	PATTERN chosen = PATTERN::LASER;
	if (m_Visits >= ETOI(PATTERN::MOVE))
		chosen = Pick_Weighted();
	else if (m_Visits > 0)
		chosen = static_cast<PATTERN>(m_Visits <= ETOI(PATTERN::LASER) ? m_Visits - 1 : m_Visits);
	if (m_Visits < ETOI(PATTERN::MOVE))
		++m_Visits;
	m_LastPattern = chosen;

	if (chosen == PATTERN::MOVE)
		Change_State(STATE::MOVE);
	else if (chosen == PATTERN::LASER)
		Change_State(STATE::LASER);
	else if (chosen == PATTERN::TAIL_STAB)
		Change_State(STATE::TAIL_STAB);
	else
	{
		const ATTACK attack = static_cast<ATTACK>(ETOI(chosen));
		m_Owner.lock()->Set_NextAttack(attack);
		Change_State(attack == ATTACK::SHOTGUN || attack == ATTACK::SPIN_FIRE ? STATE::GUN : STATE::ATTACK);
	}
}

StateEm8000_Idle::PATTERN StateEm8000_Idle::Pick_Weighted() const
{
	const Float distance = Get_DistanceToTarget();
	const Float angle = Get_AngleToTarget();
	const Bool front = angle <= FrontAngle;


	// Each pattern's weight where it can reach (em8000EmInfo, metres): the blades at 0-13 in front, the jumping scissors
	// at 13-24 in front, the jump press up to 18, the stomps up to 22, the tail sweep up to 22 in front, the tail slam at
	// 13-22 in front, the tail stab up to 18 in front or behind (likelier behind, where nothing else reaches), the laser from 18 out, the shotgun up to 40
	// and the spinning fire at 20-40. A step comes in when the player is too close or too far, and now and then anyway.
	array<Float, ETOI(PATTERN::END)> weights{};
	const auto set = [&weights](PATTERN pattern, Float weight) { weights[ETOI(pattern)] = weight; };
	if (front && distance <= 13.f)
	{
		set(PATTERN::SCISSORS, 3.f);
		set(PATTERN::BLADE_SLAM, 2.f);
	}
	if (front && distance >= 13.f && distance <= 24.f)
		set(PATTERN::JUMP_SCISSORS, 3.f);
	if (distance <= 18.f)
		set(PATTERN::JUMP_PRESS, 2.f);
	if (distance <= 22.f)
		set(PATTERN::STOMP, 2.f);
	// The tail attacks' DIR/ROT: 0520, 0530 and the front stab within 60 degrees ahead, the rear stab within 60 behind.
	if (angle <= 60.f && distance <= 22.f)
		set(PATTERN::TAIL_SWEEP, 2.f);
	if (angle <= 60.f && distance >= 13.f && distance <= 22.f)
		set(PATTERN::TAIL_SLAM, 2.f);
	if ((angle <= 60.f || angle >= 120.f) && distance <= 18.f)
		set(PATTERN::TAIL_STAB, angle >= 120.f ? 4.f : 2.f);
	if (distance >= 18.f)
		set(PATTERN::LASER, 3.f);
	if (distance <= 40.f)
		set(PATTERN::SHOTGUN, 2.f);
	if (distance >= 20.f && distance <= 40.f)
		set(PATTERN::SPIN_FIRE, 2.f);
	set(PATTERN::MOVE, distance < 6.f || distance > 26.f ? 3.f : 1.f);
	if (m_LastPattern != PATTERN::END && m_LastPattern != PATTERN::MOVE)
		weights[ETOI(m_LastPattern)] = 0.f;

	Float total = 0.f;
	for (Float weight : weights)
		total += weight;
	Float pick = static_cast<Float>(Helper::Random_Double(0.0, total));
	PATTERN chosen = PATTERN::MOVE;
	for (uint32 i = 0; i < weights.size(); ++i)
	{
		if (weights[i] <= 0.f) continue;
		chosen = static_cast<PATTERN>(i);
		if ((pick -= weights[i]) <= 0.f) break;
	}
	return chosen;
}

void StateEm8000_Idle::StateExitInvoke()
{
}

Shared<StateEm8000_Idle> StateEm8000_Idle::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Idle>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Idle");
		return nullptr;
	}

	return instance;
}
