#include "pch.h"
#include "EmCutscene.h"

#include <Game.h>

#include "Monster.h"
#include "Pl0000Input.h"
#include "ThirdPersonCamera.h"

namespace
{
	// The last cut ends looking toward the stage this far down, so the camera stays above the player's floor and the
	// whole boss is in view.
	constexpr Float PlayerCameraPitch = XMConvertToRadians(10.f);

	Float Ease(Float t)
	{
		t = std::clamp(t, 0.f, 1.f);
		return t * t * (3.f - 2.f * t);
	}
}

void EmCutscene::Begin(const Shared<Monster>& boss, const Vector3& origin, const Vector3& forward, const Shared<GameObject>& hpBar)
{
	m_Running = true;
	m_StageOrigin = origin;
	m_StageForward = Vector3{ forward.x, 0.f, forward.z };
	m_StageForward.Normalize();
	m_StageRight = Vector3::UnitY.Cross(m_StageForward);

	// The player stands facing the stage and takes no input till the end.
	if (const Shared<GameObject> player = boss->Get_Target())
	{
		m_Player = player;
		const Vector3 toStage = m_StageOrigin - player->Get_Transform()->Get_Position();
		player->Get_Transform()->Set_Rotation(Quaternion::CreateFromYawPitchRoll(atan2f(toStage.x, toStage.z), 0.f, 0.f));
		if (const Shared<Pl0000Input> input = player->Get_Component<Pl0000Input>())
		{
			m_PlayerInput = input;
			input->Set_Active(false);
		}
	}
	if (const Shared<ThirdPersonCamera> camera = Cast<ThirdPersonCamera>(GAME_INSTANCE->Get_MainCamera()))
	{
		m_Camera = camera;
		m_CameraFovY = camera->Get_FovY();
		camera->Hold(true);
	}
	// The bar floats over the boss; it would cross the close-ups.
	m_HpBar = hpBar;
	if (hpBar)
		hpBar->Set_Active(false);
}

void EmCutscene::Place_Camera(const SHOT* shots, size_t count, Float time, Float endTime, const Vector3& head, Bool pullBack) const
{
	const Shared<ThirdPersonCamera> camera = m_Camera.lock();
	if (!camera || count == 0)
		return;

	size_t index = 0;
	while (index + 1 < count && time >= shots[index + 1].start)
		++index;
	const SHOT& shot = shots[index];
	const Float end = index + 1 < count ? shots[index + 1].start : endTime;
	const Float t = Ease((time - shot.start) / (end - shot.start));

	const auto aim = [&](const Vector3& local) {
		return shot.atHead ? head + To_Stage(local) - m_StageOrigin : To_Stage(local);
	};

	Vector3 eye = To_Stage(Vector3::Lerp(shot.eyeFrom, shot.eyeTo, t));
	Vector3 at = Vector3::Lerp(aim(shot.atFrom), aim(shot.atTo), t);
	Float fovY = XMConvertToRadians(shot.fovFrom + (shot.fovTo - shot.fovFrom) * t);
	if (pullBack && index + 1 == count)
	{
		// The pull-back ends where ThirdPersonCamera would put itself to look at the stage from behind the player, at
		// its own field of view, so releasing the camera does not cut.
		const Shared<GameObject> player = m_Player.lock();
		const Vector3 pivot = (player ? player->Get_Transform()->Get_Position() : To_Stage(Vector3{ 0.f, 0.f, 20.f })) +
			Vector3{ 0.f, PlayerCameraHeight, 0.f };
		Vector3 look = m_StageOrigin - pivot;
		look.y = 0.f;
		look.Normalize();
		look = look * cosf(PlayerCameraPitch) - Vector3::UnitY * sinf(PlayerCameraPitch);
		eye = Vector3::Lerp(To_Stage(shot.eyeFrom), pivot - look * PlayerCameraDistance, t);
		at = Vector3::Lerp(aim(shot.atFrom), pivot + look * PlayerCameraDistance, t);
		fovY = XMConvertToRadians(shot.fovFrom) + (m_CameraFovY - XMConvertToRadians(shot.fovFrom)) * t;
	}

	const Shared<Transform> transform = camera->Get_Transform();
	transform->Set_Position(eye);
	transform->LookAt(at);
	transform->Update_WorldMatrix();
	camera->Set_FovY(fovY);
}

void EmCutscene::End()
{
	if (!m_Running)
		return;
	m_Running = false;
	if (const Shared<ThirdPersonCamera> camera = m_Camera.lock())
	{
		camera->Set_FovY(m_CameraFovY);
		camera->Hold(false);
	}
	if (const Shared<Pl0000Input> input = m_PlayerInput.lock())
		input->Set_Active(true);
	if (const Shared<GameObject> hpBar = m_HpBar.lock())
		hpBar->Set_Active(true);
	m_Camera.reset();
	m_PlayerInput.reset();
	m_Player.reset();
	m_HpBar.reset();
}

Vector3 EmCutscene::To_Stage(const Vector3& local) const
{
	return m_StageOrigin + m_StageRight * local.x + Vector3::UnitY * local.y + m_StageForward * local.z;
}

Bool EmCutscene::Load_Track(const filesystem::path& path)
{
	m_Track.clear();
	nlohmann::json document;
	try
	{
		ifstream file(path);
		file >> document;
	}
	catch (const std::exception&)
	{
		LOG_WARN(L"[EmCutscene] Unreadable camera track {}", path.wstring());
		return false;
	}

	const auto toVector = [](const nlohmann::json& values, size_t first) {
		return Vector3{ values[first].get<Float>(), values[first + 1].get<Float>(), values[first + 2].get<Float>() };
	};
	m_TrackFps = document.value("fps", 60.f);
	const nlohmann::json player = document.value("player", nlohmann::json::array({ 0.f, 0.f, 0.f }));
	m_TrackPlayer = toVector(player, 0);
	for (const auto& key : document.value("keys", nlohmann::json::array()))
		m_Track.push_back(TRACK_KEY{ toVector(key, 0), toVector(key, 3), key[6].get<Float>() });
	return !m_Track.empty();
}

void EmCutscene::Place_Camera(Float time) const
{
	const Shared<ThirdPersonCamera> camera = m_Camera.lock();
	if (!camera || m_Track.empty())
		return;

	const Float frame = std::clamp(time * m_TrackFps, 0.f, static_cast<Float>(m_Track.size() - 1));
	const size_t index = static_cast<size_t>(frame);
	const TRACK_KEY& from = m_Track[index];
	const TRACK_KEY& to = m_Track[(std::min)(index + 1, m_Track.size() - 1)];
	const Float t = frame - static_cast<Float>(index);

	const Shared<Transform> transform = camera->Get_Transform();
	transform->Set_Position(To_Stage(Vector3::Lerp(from.eye, to.eye, t)));
	transform->LookAt(To_Stage(Vector3::Lerp(from.at, to.at, t)));
	transform->Update_WorldMatrix();
	camera->Set_FovY(from.fovY + (to.fovY - from.fovY) * t);
}

Float EmCutscene::Get_TrackLength() const
{
	return m_Track.empty() ? 0.f : static_cast<Float>(m_Track.size()) / m_TrackFps;
}

void EmCutscene::Place_Player() const
{
	const Shared<GameObject> player = m_Player.lock();
	if (!player)
		return;
	const Shared<Transform> transform = player->Get_Transform();
	const Vector3 spot = To_Stage(m_TrackPlayer);
	transform->Set_Position(spot);
	const Vector3 toStage = m_StageOrigin - spot;
	transform->Set_Rotation(Quaternion::CreateFromYawPitchRoll(atan2f(toStage.x, toStage.z), 0.f, 0.f));
	transform->Update_WorldMatrix();
}
