#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

/* The tail stab (em8000EmInfo 前方用反転尻尾突き刺し: 0555 -> 0556 looped TIME_120 MOT_WAIT -> 0557, TURN_DIR_180_180;
   後方尻尾突き刺し with the player behind: 055A -> 0556 -> 0557). From the front it turns half round on the spot to bring
   the tail's drill onto where the player stood (0555); from behind the drill already points there (055A). Then the drill
   shoots out and back (0556) until StabTime has passed and the lap in hand ends (three stabs), the boss turning on the
   spot toward the player at Em8000::Get_TailTurnSpeed meanwhile, and it turns back to face the player (0557). The half
   turns are in the clips' root rotation, so the boss takes the body's facing when a clip ends and the next clip starts
   from the same pose without a blend. */
class CLIENT_DLL StateEm8000_TailStab final : public StateEm8000
{
public:
	explicit StateEm8000_TailStab(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_TailStab() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	// Turns the boss on the spot so its tail end comes toward the player, no faster than the Inspector speed.
	void Turn_TailToTarget(Float timeDelta) const;

private:
	Float m_Time{};

	static constexpr Float StabTime = 120.f / 60.f;		// TIME_120: 0556 laps on until this, then ends its lap
	static constexpr Float StabLap = 54.f / 60.f;		// one lap of 0556 (its .anim runs 54 ticks at 60 a second)
	static constexpr Float BehindAngle = 120.f;			// DIR_180 ROT_60: the player this far round starts 055A

public:
	static Shared<StateEm8000_TailStab> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
