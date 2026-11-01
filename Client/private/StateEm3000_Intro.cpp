#include "pch.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "StateEm3000_Intro.h"

#include <Game.h>

#include "BossAlert.h"
#include "Em3000Body.h"
#include "EmMovement.h"
#include "EmRing.h"
#include "EmTelegraph.h"
#include "Model.h"
#include "Pl0000.h"

namespace
{
	// One cut of the opening in the boss's frame up on the stage (x right, y up, z forward toward the player); the eye
	// and the aim ease from the first value to the second over the cut, and an aim on the head is an offset from the head
	// bone, so it follows the clip. In place of the original's opening movie (reference video 0:15~0:24): the ceiling
	// tilt-down, the dark stage, the spots on the dress and its dolls, the face, all while the boss stands idle. The event
	// camera takes over at EventTime.
	const EmCutscene::SHOT Shots[]
	{
		{ 0.f, { 3.f, 2.f, 15.f }, { 3.f, 2.f, 14.f }, { 0.f, 26.f, 2.f }, { 0.f, 6.5f, 0.f }, 60.f, 60.f, false },
		{ 3.f, { 0.f, 6.f, 22.f }, { 0.f, 5.f, 18.5f }, { 0.f, 5.f, 0.f }, { 0.f, 4.5f, 0.f }, 55.f, 55.f, false },
		{ 6.f, { -5.f, 2.f, 6.5f }, { -3.f, 2.5f, 6.f }, { -1.f, 3.f, 0.f }, { 0.f, 3.5f, 0.f }, 50.f, 50.f, false },
		{ 8.5f, { -4.f, 3.5f, 4.f }, { 4.f, 3.8f, 4.f }, { -1.5f, 3.8f, 0.f }, { 1.5f, 3.8f, 0.f }, 40.f, 40.f, false },
		{ 11.f, { 0.8f, 6.f, 4.f }, { 0.f, 6.2f, 3.2f }, { 0.f, 0.f, 0.f }, { 0.f, 0.1f, 0.f }, 35.f, 30.f, true },
	};
	constexpr const tChar* EventCameraFile = L"ev0301_camera.json";	// next to em3000.model
	constexpr Float WarningVolume = 0.6f;

	/* ev0301's screen effects (reference video 0:25.6~0:34), in event frames. The boss throws out one arm and then the
	   other with a shout (em3000_00_000's hand bones fly out from 168 and 248), each glitching the screen and sending one
	   ripple out of its head. Then it throws out both (376) and roars until it lowers them (about 680): a ripple every
	   RoarRippleEvery frames while the screen's colours stay split, as strong as the shake ev0301_VibSeq runs at that point
	   (0.3 is its strongest). The event's ScrSeq is empty, so the look is tuned on screen against the reference. */
	constexpr Float ThrustFrames[]{ 172.f, 252.f };
	constexpr Float ThrustGlitch = 0.8f;
	constexpr Float ThrustGlitchTime = 0.35f;
	constexpr Float ThrustRippleRadius = 16.f;
	constexpr pair<Float, Float> RoarShakes[]{ { 375.f, 0.3f }, { 420.f, 0.3f }, { 475.f, 0.3f }, { 532.f, 0.3f },
		{ 588.f, 0.15f }, { 619.f, 0.1f } };
	constexpr Float RoarEnd = 680.f;
	constexpr Float RoarRippleEvery = 24.f;
	constexpr Float RoarRippleRadius = 12.f;
	constexpr Float RoarColourSplit = 0.35f;
	// Each roar ripple also glitches the screen a little, above the split so its bands show, as strong as the shake.
	constexpr Float RoarGlitch = 0.5f;
	constexpr Float RoarGlitchTime = 0.15f;
	constexpr Float RippleTime = 0.9f;		// a ripple opens out over this long
	constexpr Float RippleWidth = 3.f;
}

StateEm3000_Intro::StateEm3000_Intro(const wstring& tag, const Shared<Em3000>& owner)
	: StateEm3000{ tag, owner } {}

Bool StateEm3000_Intro::StateEnterInvoke()
{
	m_Time = 0.f;
	m_HoverClipTime = -1.f;
	Play(ANIM::IDLE, true);
	return true;
}

void StateEm3000_Intro::Update(Float timeDelta)
{
	if (!m_Cutscene.Is_Running() && m_HoverClipTime < 0.f && m_Time <= 0.f)
	{
		if (!Has_Target()) return;
		Begin_Cutscene();
		return;
	}

	const Float previous = m_Time;
	m_Time += timeDelta;
	if (previous < FaceFlashTime && m_Time >= FaceFlashTime)
		if (const Shared<Em3000Body> body = m_Body.lock())
			EmTelegraph::Play(body, body->Get_ModelComponent(), FaceFlashSize, L"Em_Tele_L");
	if (previous < EventTime && m_Time >= EventTime)
	{
		Play(ANIM::EVENT_INTRO, false, 0.3f);
		// Without the event camera the cutscene stops here rather than hold the last cut.
		if (!m_HasEventCamera)
			m_Cutscene.End();
	}
	if (previous < SingTime && m_Time >= SingTime)
		GAME_INSTANCE->PlaySoundFX(L"BGM_Bt_Opera_Intro", Em3000::BgmIntroChannel, Em3000::BgmVolume);
	if (previous < BgmTime && m_Time >= BgmTime)
		Start_Bgm();
	Play_EventEffects(previous);
	if (previous < WarningTime && m_Time >= WarningTime)
	{
		m_Cutscene.End();
		Begin_Hover();
	}
	if (m_Cutscene.Is_Running())
		Place_Camera();

	if (m_HoverClipTime < 0.f)
		return;
	const Float rate = Get_HoverRate(m_HoverClipTime);
	m_Body.lock()->Set_AnimationRate(rate);
	m_HoverClipTime += timeDelta * rate;
	if (Is_Finished(ANIM::HOVER_IN))
		Change_State(STATE::IDLE);
}

void StateEm3000_Intro::StateExitInvoke()
{
	m_Body.lock()->Set_AnimationRate(1.f);
	m_Movement.lock()->Set_MovementData({});	// drops the hover's flight; Idle sets none of its own
	m_Cutscene.End();
	// Cut short (the boss state changed early), the stage still ends up lit on the cast.
	if (const Shared<Em3000> owner = m_Owner.lock(); owner && m_Time > 0.f)
	{
		owner->Light_StageSpots(m_Stage, m_Forward);
		owner->Cast_StageSpots();
		if (m_Time < BgmTime)
			Start_Bgm();
	}
}

void StateEm3000_Intro::Start_Bgm()
{
	GAME_INSTANCE->PlaySoundLoopSection(L"BGM_Bt_Opera", Em3000::BgmChannel, Em3000::BgmVolume, 0, UINT32_MAX, true);
}

void StateEm3000_Intro::Begin_Cutscene()
{
	const Shared<Em3000> owner = m_Owner.lock();
	const Shared<Transform> transform = owner->Get_Transform();
	transform->Update_WorldMatrix();
	m_Stage = transform->Get_Position();
	m_Forward = transform->Get_Look();
	m_Forward.y = 0.f;
	m_Forward.Normalize();

	const Shared<Model> model = m_Body.lock()->Get_ModelComponent();
	m_HeadBoneIndex = model->Get_BoneIndexByName("bone16");
	m_Cutscene.Begin(owner, m_Stage, m_Forward, owner->Find_Child(L"Em3000HpBar"));
	// The opera takes over from the park's music, which comes back once the boss breaks up (StateEm3000_Dead).
	GAME_INSTANCE->FadeSound(Pl0000::LevelBgmChannel, 0.f, Pl0000::LevelBgmFadeOutTime);
	m_HasEventCamera = m_Cutscene.Load_Track(filesystem::path(model->Get_ModelFilePath()).parent_path() / EventCameraFile);
	if (m_HasEventCamera)
		m_Cutscene.Place_Player();
	m_Time = 0.f;
	Place_Camera();
}

void StateEm3000_Intro::Place_Camera() const
{
	if (m_Time >= EventTime)
	{
		m_Cutscene.Place_Camera(m_Time - EventTime);
		return;
	}
	m_Cutscene.Place_Camera(Shots, size(Shots), m_Time, EventTime, Get_HeadPosition(), false);
}

Vector3 StateEm3000_Intro::Get_HeadPosition() const
{
	if (const Shared<Em3000Body> body = m_Body.lock(); body && m_HeadBoneIndex != -1)
		return (body->Get_ModelComponent()->Get_BoneMatrix(m_HeadBoneIndex) * body->Get_CombinedWorldMatrix()).Translation();
	return m_Cutscene.To_Stage(Vector3{ 0.f, 6.9f, 0.f });
}

void StateEm3000_Intro::Play_EventEffects(Float previous) const
{
	const Float from = (previous - EventTime) * EventFps, to = (m_Time - EventTime) * EventFps;
	for (const Float frame : ThrustFrames)
		if (from < frame && to >= frame)
		{
			GAME_INSTANCE->Start_ScreenGlitch(ThrustGlitch, ThrustGlitchTime);
			Spread_Ripple(ThrustRippleRadius, 1.f);
		}

	const Float roarStart = RoarShakes[0].first;
	if (to < roarStart || to >= RoarEnd)
		return;
	Float force = 0.f;
	for (const auto& [frame, shake] : RoarShakes)
		if (to >= frame)
			force = shake;
	const Float strength = force / RoarShakes[0].second;
	GAME_INSTANCE->Start_ScreenGlitch(RoarColourSplit * strength, 0.2f, false);
	if (from < roarStart || floorf((from - roarStart) / RoarRippleEvery) != floorf((to - roarStart) / RoarRippleEvery))
	{
		GAME_INSTANCE->Start_ScreenGlitch(RoarGlitch * strength, RoarGlitchTime);
		Spread_Ripple(RoarRippleRadius, strength);
	}
}

void StateEm3000_Intro::Spread_Ripple(Float endRadius, Float strength) const
{
	const Shared<Em3000> owner = m_Owner.lock();
	if (!owner) return;
	EmRing::EM_RING_DESC desc{};
	desc.mode = EmRing::MODE::ROAR;
	desc.start = Get_HeadPosition();
	desc.endRadius = endRadius;
	desc.spreadSpeed = endRadius / RippleTime;
	desc.bandWidth = RippleWidth;
	desc.haze = strength;
	GAME_INSTANCE->Instantiate<EmRing>(L"EmRing", owner->Get_LevelIndex(), &desc);
}

void StateEm3000_Intro::Begin_Hover()
{
	const Shared<Em3000> owner = m_Owner.lock();
	owner->Light_StageSpots(m_Stage, m_Forward);
	owner->Cast_StageSpots();
	GAME_INSTANCE->PlaySoundFXOnce(L"em3000_spot_on", SOUNDCHANNEL::CHANNEL_26, WarningVolume);
	// The WARNING plays core_warning and keeps the bar (shown by the cutscene's end) hidden until it lands in it.
	BossAlert::Play(owner);
	if (const auto client = ClientApp::GetInstance())
		client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM3000_ENCOUNTER);

	// The root motion, stretched over the distance, carries it forward and, flying, down off the stage's edge onto the
	// floor as the clip does.
	EmMovement::EM_MOVEMENT_DATA data{};
	data.isMove = true;
	data.canRotation = false;
	data.rootMotionScale = StageDistance / HoverRootDistance;
	data.followRootHeight = true;
	m_Movement.lock()->Set_MovementData(data);
	Play(ANIM::HOVER_IN, false, 0.3f);
	m_HoverClipTime = 0.f;
}

Float StateEm3000_Intro::Get_HoverRate(Float clipTime)
{
	if (clipTime >= 0.75f && clipTime < 2.5f)
		return 0.44f;
	if (clipTime >= 2.85f)
		return 0.17f;
	return 1.f;
}

Shared<StateEm3000_Intro> StateEm3000_Intro::Create(const wstring& tag, const Shared<Em3000>& owner)
{
	auto instance = make_shared<StateEm3000_Intro>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm3000_Intro");
		return nullptr;
	}

	return instance;
}
