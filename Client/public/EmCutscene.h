#pragma once
#include "Client_Define.h"

NS_BEGIN(Client)

class Monster;
class Pl0000Input;
class ThirdPersonCamera;

/* The big bosses' entrance cutscene (Em3000, Em8000), run by their intro states: from Begin to End the player stands
   facing the stage with no input, the boss's HP bar is hidden, and the third-person camera is held and follows a shot
   list given in the stage's frame (x right, y up, z forward toward the player). The last shot pulls back onto where
   ThirdPersonCamera would sit behind the player looking at the stage, at its own field of view, so letting go does not
   cut. A boss with the original event camera plays that track instead (Load_Track). */
class CLIENT_DLL EmCutscene final
{
public:
	// One cut: the eye and the aim ease from the first value to the second over the cut. An aim on the head is an offset
	// from the head point the state hands in, so it follows the clip.
	struct SHOT
	{
		Float start;
		Vector3 eyeFrom, eyeTo;
		Vector3 atFrom, atTo;
		Float fovFrom, fovTo;	// degrees
		Bool atHead;
	};

public:
	// origin and forward: the stage's frame on the floor.
	void Begin(const Shared<Monster>& boss, const Vector3& origin, const Vector3& forward, const Shared<GameObject>& hpBar);
	// Places the camera `time` seconds in; the last shot ends at endTime and, with pullBack, eases onto the player's camera.
	void Place_Camera(const SHOT* shots, size_t count, Float time, Float endTime, const Vector3& head, Bool pullBack = true) const;
	void End();
	Bool Is_Running() const { return m_Running; }
	// A point given in the stage's frame.
	Vector3 To_Stage(const Vector3& local) const;

public:
	// An original event camera in the stage's frame (Models/em3000/ev0301_camera.json, made by make_event_camera.py): eye,
	// aim and vertical field of view per frame, and where the event stands the player. False if the file is unreadable.
	Bool Load_Track(const filesystem::path& path);
	// Places the camera `time` seconds into the track (held on its last frame after the end).
	void Place_Camera(Float time) const;
	Float Get_TrackLength() const;
	// Moves the player to the track's spot, facing the stage; the movement's controller follows on its next move.
	void Place_Player() const;

private:
	struct TRACK_KEY
	{
		Vector3 eye;
		Vector3 at;
		Float fovY;
	};
	vector<TRACK_KEY> m_Track;
	Float m_TrackFps{ 60.f };
	Vector3 m_TrackPlayer{};

private:
	Bool m_Running{ false };
	Vector3 m_StageOrigin{};
	Vector3 m_StageRight{};
	Vector3 m_StageForward{};
	Weak<ThirdPersonCamera> m_Camera{};
	Float m_CameraFovY{};
	Weak<GameObject> m_Player{};
	Weak<Pl0000Input> m_PlayerInput{};
	Weak<GameObject> m_HpBar{};

	static constexpr Float PlayerCameraDistance = 7.f;	// ThirdPersonCamera's default distance and pivot height
	static constexpr Float PlayerCameraHeight = 1.f;
};

NS_END
