#pragma once
#include "StateEm8000.h"
#include "EmCutscene.h"

NS_BEGIN(Client)

/* The entrance: the boss waits hanging on the building beside the arena (2050) until the player comes within sight of the
   arena, then, as a cutscene (EmCutscene), jumps down onto where it was placed (2051), sending its shockwave over the
   floor as it lands, and the fight starts.
   2051's root carries the boss JumpTravel to its side while it turns a quarter round, and in 2050 its toes grip a wall
   WallGap on from where the jump starts. So the jump starts against the wall a ray finds on that side of the placed spot,
   and its travel is stretched (or shortened) to the distance from there, so it lands on the placed spot. The boss itself (its feet, the character controller) stands OwnerClear out from the wall and the body is drawn
   shifted onto it; the sight stays over the arena. The root motion moves the boss directly (swept along the floor it
   would catch on the rocks), and when the clip ends, on 0000's first pose, the boss takes the body's place and turns to
   where it faces. */
class CLIENT_DLL StateEm8000_Intro final : public StateEm8000
{
public:
	explicit StateEm8000_Intro(const wstring& tag, const Shared<Em8000>& owner);
	~StateEm8000_Intro() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	// Moves the boss from where it was placed to the foot of its wall and hangs it there.
	void Hang();
	// Ends the jump: the boss stands where the body landed, facing where it faces, and the fight starts.
	void Land();
	void Place_Camera() const;
	// Where a child that follows the boss (its body, its sight) sits from it, given in the world's axes.
	void Set_ChildAt(const wstring& child, const Vector3& worldOffset) const;

private:
	Bool m_Hanging{ false };
	EmCutscene m_Cutscene{};	// staged at the placed spot, z forward along the placed facing (the player's way in)
	Float m_Time{};
	Vector3 m_Landing{};		// where the boss was placed
	Vector3 m_BodyShift{};		// the body drawn this far from the boss while it is on the wall and in the air
	Float m_JumpScale{ 1.f };	// 2051's travel times this reaches the placed spot from the wall
	int32 m_BodyBoneIndex{ -1 };

	static constexpr Float JumpTravel = 36.24f;		// 2051's root motion, metres to the boss's side
	static constexpr Float WallGap = 1.2f;			// 2050: the toes' plane, this far from the jump's start toward its end
	static constexpr Float OwnerClear = 2.f;		// the boss's feet stay this far out from the wall
	static constexpr Float HangHeight = 26.f;		// 2050: bone1's height over the boss's feet
	static constexpr Float WallSearch = 60.f;

public:
	static Shared<StateEm8000_Intro> Create(const wstring& tag, const Shared<Em8000>& owner);
};

NS_END
