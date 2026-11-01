#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

// Plays the one-shot attack the idle state picked (Em8000::Get_NextAttack): the blade scissors (0600) or slam (0610),
// the jumping scissors (0605), the jump press (0500), one of the two stomp orders (0560/0561) or the tail swung round
// (0520). It steers toward the player through the wind-up and follows the clip's root motion; the hits are timed by
// Em8000Body's notifies.
class CLIENT_DLL StateEm8000_Attack final : public StateEm8000
{
public:
	explicit StateEm8000_Attack(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Attack() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	ANIM m_Clip{ ANIM::SCISSORS };
	Float m_Elapsed{};
	Float m_SteerTime{};

	// Steering stops a little before each clip's first hit frame (seq AttackTrack), so the swing can be dodged.
	static constexpr Float BladeSteerTime = 1.3f;		// 0600/0610 hit at 98 (1.63 s)
	static constexpr Float JumpSteerTime = 1.5f;		// 0605 hits at 112 (1.87 s)
	static constexpr Float StompSteerTime = 0.5f;		// 0500 lands at 129; the stomps turn the body themselves
	static constexpr Float SlamSteerTime = 0.8f;		// 0530 comes down at 65 (1.08 s)
	static constexpr Float TailSteerTime = 1.f;		// 0520 swings at 115 (1.92 s); its root turns the body on round

public:
	static Shared<StateEm8000_Attack> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
