#pragma once
#include "StateEm3000.h"
#include "EmCutscene.h"

NS_BEGIN(Client)

// The entrance, after the reference video 0:15~0:43. The boss is placed where it waits up on the stage (the event's spot,
// (1180.9, 22.35, 225.1)) until the player is found; its sight (MonsterSight, offset in the level) watches the floor
// where it will land, StageDistance ahead. Then a cutscene (EmCutscene) holds the player's input and the third-person camera and
// stands the player where the original event does: the boss stands idle through our opening cuts (the original plays a
// movie there, ev0300.usm), then the original event ev0301 runs (the boss's event clip, the event camera, the soprano
// and the opera at the frames of its BgmSeq). When it ends the camera returns to the player with the WARNING: the stage
// spots come on over the player and the boss with the spot and warning sounds, and the boss hovers forward off the stage onto its place (e001 at the original seq's SpeedTrack, following the
// clip's height, its jets humming); phase 1 starts when it lands.
class CLIENT_DLL StateEm3000_Intro final : public StateEm3000
{
public:
	explicit StateEm3000_Intro(const wstring& tag, const Shared<Em3000>& owner);
	~StateEm3000_Intro() override = default;

public:
	Bool StateEnterInvoke() override;
	void Update(Float timeDelta) override;
	void StateExitInvoke() override;

private:
	void Begin_Cutscene();
	void Place_Camera() const;
	void Begin_Hover();
	// ev0301's screen effects from the last update's time up to now (see ThrustFrames and RoarShakes in the source).
	void Play_EventEffects(Float previous) const;
	// One O-shaped ripple (EmRing ROAR) out of the head.
	void Spread_Ripple(Float endRadius, Float strength) const;
	Vector3 Get_HeadPosition() const;
	// The opera comes in at BgmTime and loops through the fight.
	static void Start_Bgm();
	// e001 seq SpeedTrack: the playback rate at this many clip seconds in.
	static Float Get_HoverRate(Float clipTime);

private:
	EmCutscene m_Cutscene{};	// staged in the boss's frame up on the stage (z forward, toward the player)
	Bool m_HasEventCamera{ false };
	Float m_Time{};
	Float m_HoverClipTime{ -1.f };	// < 0 until the hover starts
	Vector3 m_Stage{};			// where it was placed and waits up on the stage; the stage spots hang in this frame
	Vector3 m_Forward{};
	int32 m_HeadBoneIndex{ -1 };

	static constexpr Float StageDistance = 20.f;	// from the event's spot on the stage out into the dome
	static constexpr Float HoverRootDistance = 9.f;	// e001's root channel forward, stretched to StageDistance
	static constexpr Float EventTime = 14.f;		// the face cut ends and ev0301 starts (reference 0:22.9)
	// In the face cut (11 s on) the eyes flash (EmTelegraph's streak and Em_Tele_L) once the mask fills the screen
	// (reference 0:21.0), on request; FaceFlashSize lifts the streak off the big head toward the camera.
	static constexpr Float FaceFlashTime = 12.2f;
	static constexpr Float FaceFlashSize = 3.f;
	static constexpr Float EventFps = 60.f;
	// ev0301_BgmSeq readies the soprano lead-in at 110 (BGM_Play_OperaVoice_A); it is heard from BGM_Vocal_On at 172, on the
	// first wide sweep of the arms (reference 0:25.74), and the opera comes in on the next bar, 1.06 s later (0:26.8).
	static constexpr Float SingTime = EventTime + 172.f / EventFps;
	static constexpr Float BgmTime = SingTime + 1.06f;
	static constexpr Float WarningTime = EventTime + 802.f / EventFps;	// back to the player, the WARNING and the hover

public:
	static Shared<StateEm3000_Intro> Create(const wstring& tag, const Shared<Em3000>& owner);
};

NS_END
