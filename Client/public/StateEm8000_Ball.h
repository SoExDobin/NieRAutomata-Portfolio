#pragma once
#include "StateEm8000.h"

NS_BEGIN(Client)

/* The last stand once HP is down to Em8000::Get_RageHpRatio (entered from the idle state, the boss stays in it to the end).
   0540 throws the armour off and curls it up into the ball, then it goes round a fixed order with a pause facing the
   player (0542) between attacks: the U-turn dash twice, the ball jump press twice and the spin, over and over (on
   request, 2026-10-16).
   - U-turn dash (em8000EmInfo 球体：突進攻撃Ｕターン): 0545 winds up facing the player, 0546 rolls one lap straight on,
     0547 turns back, 0546 rolls at the player again, steering to them as it sets off (on request, 2026-10-22, not just
     back the way it came), 0548 brakes and 0320 wobbles dizzy.
   - Ball jump press (ブレードタイプ球体：ジャンププレス 054A): it faces the player until it jumps, then homes on where
     the player is through the air and lands on them (the clip's own 12 m are stretched or shrunk frame by frame); the
     landing sends a ring over the floor. It jumps Em8000::Get_BallPressLift higher than the clip.
   - Spin (球体：回転攻撃): 054B, 054C laps on until SpinTime has passed and the lap in hand ends (TIME_180 MOT_WAIT),
     054D. While 054C laps it sprays RingShots orbs from its eye every RingInterval, scattered in a cone round the way
     the eye faces, so the volleys sweep round with the spin (on request, 2026-10-24); every fourth volley cannot be
     cut, as Em3000's barrage.
   The hits and sounds are timed by Em8000Body's notifies. */
class CLIENT_DLL StateEm8000_Ball final : public StateEm8000
{
public:
	explicit StateEm8000_Ball(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Ball() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	enum class PATTERN { UTURN, PRESS, SPIN };
	// Plays clip and notes it and when it started. The model reports the clip it blends out of until a blend ends, so
	// the steps go by m_Clip.
	void Start(ANIM clip, Bool loop = false, Float blend = 0.f);
	void Start_Next();
	void Update_Dash();
	void Update_Press();

private:
	ANIM m_Clip{ ANIM::BALL_IN };
	Float m_Elapsed{};			// since the current clip started
	Float m_Wait{};				// left of the pause before the next attack
	Float m_SpinTime{};
	Float m_RingTimer{};		// left until the spin's next volley
	uint32 m_Rings{};			// thrown in this spin
	Bool m_Returning{};			// the U-turn dash's second roll
	uint32 m_Next{};			// into Cycle

	static constexpr PATTERN Cycle[] = { PATTERN::UTURN, PATTERN::UTURN, PATTERN::PRESS, PATTERN::PRESS, PATTERN::SPIN };
	static constexpr Float WaitTime = 0.8f;
	static constexpr Float DashSteerTime = 44.f / 60.f;		// 0545 sets off (its hit goes on) at 44
	static constexpr Float ReturnSteerTime = 1.f;			// the second 0546 turns to the player this long (turnSpeed 2.5: ~90 %)
	static constexpr Float PressJumpFrame = 24.f;			// 054A leaves the floor at 24
	static constexpr Float PressLandFrame = 91.f;			// and lands at 91 (its wide landing sphere)
	static constexpr Float PressTopFrame = 60.f;			// its ball is in the air from 30 to 90, highest at 60
	static constexpr Float PressAirHalf = 30.f;
	static constexpr Float PressRootSpeed = 9.73f / 73.f;	// its root runs at an even pace from 32 (2.27 m) to 105 (12 m)
	static constexpr Float PressMaxScale = 4.f;				// at most 4x the clip's pace (about 32 m/s like the roll, 35 m in one jump)
	static constexpr Float NearTarget = 2.f;				// closer than this the player is under the ball: no turning
	static constexpr Float SpinTime = 180.f / 60.f;			// TIME_180
	static constexpr Float SpinLap = 60.f / 60.f;			// one lap of 054C (its .anim runs 60 ticks at 60 a second)
	static constexpr uint32 RingShots = 16;
	static constexpr Float RingInterval = 0.15f;			// about 20 volleys, 320 orbs in a spin

public:
	static Shared<StateEm8000_Ball> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
