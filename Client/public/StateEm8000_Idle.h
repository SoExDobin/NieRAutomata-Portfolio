#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

// Stands still until it sees the player, then turns to face it and after a short pause picks the next pattern by range
// and bearing (em8000EmInfo MIN/MAX in cm and DIR/ROT), never the same one twice in a row (NO_2TIME).
class CLIENT_DLL StateEm8000_Idle final : public StateEm8000
{
public:
	explicit StateEm8000_Idle(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Idle() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	// The patterns it picks from; ATTACK values first, then those that have their own states.
	enum class PATTERN { SCISSORS, BLADE_SLAM, JUMP_SCISSORS, JUMP_PRESS, STOMP, TAIL_SWEEP, TAIL_SLAM, SHOTGUN, SPIN_FIRE,
		LASER, TAIL_STAB, MOVE, END };
	void Choose_Next();
	PATTERN Pick_Weighted() const;

private:
	Float m_Delay{};
	PATTERN m_LastPattern{ PATTERN::END };
	uint32 m_Visits{};	// patterns picked, for the first round

	static constexpr Double MinDelay = 0.8;
	static constexpr Double MaxDelay = 1.8;
	static constexpr Float FrontAngle = 35.f;		// the blade attacks need the player within this of straight ahead

public:
	static Shared<StateEm8000_Idle> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
