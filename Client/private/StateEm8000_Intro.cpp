#include "pch.h"
#include "StateEm8000_Intro.h"

#include <Game.h>

#include "BossAlert.h"
#include "ClientApp.h"
#include "UIManager.h"
#include "Em8000Body.h"
#include "Model.h"
#include "Pl0000.h"

namespace
{
	// The cuts in the stage's frame (x right, toward the building; y up; z forward, toward the player's way in). The boss
	// hangs some 46 m to the right, 26 m up at (500, -10) (its wall); the aims on the head follow the body (bone3) through
	// the jump. The eyes stand where no tree or bank is between them and the boss (the tiles' bgd00b / bg2043 props and
	// COL, checked by script), as the trees round the landing hid it from the first spots.
	const EmCutscene::SHOT Shots[]
	{
		// From the way in: the building and the boss hanging on it.
		{ 0.f, { -2.f, 4.f, 22.f }, { 0.f, 5.f, 20.f }, { 0.f, -4.f, 0.f }, { 0.f, -3.f, 0.f }, 45.f, 42.f, true },
		// Under it, looking up as it stirs.
		{ 2.5f, { 34.f, 7.f, 2.f }, { 36.f, 8.f, -2.f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, 50.f, 45.f, true },
		// The jump and the fall, held wide.
		{ 5.f, { 6.f, 7.f, 10.f }, { 6.f, 7.f, 14.f }, { 0.f, 0.f, 0.f }, { 0.f, -1.f, 0.f }, 60.f, 60.f, true },
		// The landing, low in front.
		{ 6.6f, { -10.f, 1.5f, 12.f }, { -9.f, 2.f, 13.f }, { 0.f, -2.f, 0.f }, { 0.f, -2.f, 0.f }, 50.f, 50.f, true },
		{ 7.8f, { -6.f, 3.f, 16.f }, {}, { 0.f, -2.f, 0.f }, {}, 50.f, 0.f, true },	// ends on the player's camera
	};
	constexpr Float JumpTime = 5.f;					// 2051 starts with the jump cut
	constexpr Float EndTime = JumpTime + 270.f / 60.f;	// 2051: 270 ticks at 60 per second
}

StateEm8000_Intro::StateEm8000_Intro(const wstring& tag, const Shared<Em8000>& owner)
	: StateEm8000{ tag, owner } {}

Bool StateEm8000_Intro::StateEnterInvoke()
{
	// Entered while the boss is still being made, before the scene places it: it hangs on its first update.
	m_Hanging = false;
	m_Time = 0.f;
	Play(ANIM::IDLE, true, 0.f);
	return true;
}

void StateEm8000_Intro::Update(Float timeDelta)
{
	Hold_Still();
	if (!m_Hanging)
	{
		Hang();
		return;
	}
	const Shared<Em8000> owner = m_Owner.lock();
	if (!m_Cutscene.Is_Running())
	{
		if (!Has_Target()) return;
		m_Cutscene.Begin(owner, m_Landing, owner->Get_Transform()->Get_Look(), owner->Find_Child(L"Em8000HpBar"));
		GAME_INSTANCE->FadeSound(Pl0000::LevelBgmChannel, 0.f, Pl0000::LevelBgmFadeOutTime);
		Place_Camera();
		return;
	}

	const Float previous = m_Time;
	m_Time += timeDelta;
	if (previous < JumpTime && m_Time >= JumpTime)
	{
		Play(ANIM::INTRO_JUMP, false, 0.3f);
		m_Body.lock()->Start_Hum();
	}
	if (Is_Playing(ANIM::INTRO_JUMP))
	{
		// The jump's root motion moves the boss straight (the controller only finds the floor under it).
		const Shared<Transform> transform = owner->Get_Transform();
		const Vector3 delta = owner->Get_BodyModelTransform().position;
		transform->Set_Position(transform->Get_Position() +
			Vector3::Transform(Vector3{ delta.x, 0.f, delta.z } * m_JumpScale, transform->Get_Quaternion()));
		transform->Update_WorldMatrix();
	}
	Place_Camera();
	if (Is_Finished(ANIM::INTRO_JUMP))
		Land();
}

void StateEm8000_Intro::StateExitInvoke()
{
	// Cut short (killed on the wall), the boss takes the body's place.
	if (const Shared<Em8000> owner = m_Owner.lock(); owner && m_BodyShift.LengthSquared() > 0.f)
	{
		owner->Get_Transform()->Set_Position(owner->Get_Transform()->Get_Position() + m_BodyShift);
		m_BodyShift = Vector3::Zero;
		Set_ChildAt(L"Em8000Body", Vector3::Zero);
	}
	Set_ChildAt(L"Em8000Sight", Vector3::Zero);
	m_Cutscene.End();
	// Landed or cut short, the fight starts with its music and the WARNING, which shows the bar as it lands in it;
	// killed on the wall, the death brings the level's music back.
	if (const Shared<Em8000> owner = m_Owner.lock(); owner && !owner->Is_Dead())
	{
		BossAlert::Play(owner);
		if (const auto client = ClientApp::GetInstance())
			client->Get_UIManager()->Queue_Scenario(COMMUNICATION_CUE::EM8000_ENCOUNTER);
		GAME_INSTANCE->FadeSound(Pl0000::LevelBgmChannel, 0.f, Pl0000::LevelBgmFadeOutTime);
		Em8000::Start_Bgm();
		// Cut short before the jump, the hum starts with the fight.
		m_Body.lock()->Start_Hum();
	}
}

void StateEm8000_Intro::Hang()
{
	m_Hanging = true;
	const Shared<Em8000> owner = m_Owner.lock();
	const Shared<Transform> transform = owner->Get_Transform();
	transform->Update_WorldMatrix();
	m_Landing = transform->Get_Position();
	m_BodyBoneIndex = m_Body.lock()->Get_ModelComponent()->Get_BoneIndexByName("bone3");

	// 2051 carries the boss to its side (its look turned a quarter to the right); the wall is the other way.
	Vector3 look = transform->Get_Look();
	look.y = 0.f;
	look.Normalize();
	const Vector3 side{ -look.z, 0.f, look.x };
	Float wall = JumpTravel - WallGap;
	Vector3 hit{};
	if (GAME_INSTANCE->Raycast_Physics(m_Landing + Vector3{ 0.f, HangHeight, 0.f }, -side, WallSearch, hit))
		wall = Vector2{ hit.x - m_Landing.x, hit.z - m_Landing.z }.Length();
	const Vector3 jumpStart = m_Landing - side * (wall + WallGap);
	m_JumpScale = (wall + WallGap) / JumpTravel;
	const Vector3 feet = m_Landing - side * (wall - OwnerClear);
	m_BodyShift = jumpStart - feet;
	transform->Set_Position(feet);
	transform->Update_WorldMatrix();
	Set_ChildAt(L"Em8000Body", m_BodyShift);
	Set_ChildAt(L"Em8000Sight", m_Landing - feet);
	// No bar over an empty floor while it waits on the wall.
	if (const Shared<GameObject> hpBar = owner->Find_Child(L"Em8000HpBar"))
		hpBar->Set_Active(false);
	Play(ANIM::INTRO_HANG, true, 0.f);
}

void StateEm8000_Intro::Land()
{
	// The clip ends on 0000's first pose turned a quarter round: the boss stands where the body landed and turns to
	// where it faces, so the idle follows on without a blend.
	Turn_ToBody();
	const Shared<Transform> transform = m_Owner.lock()->Get_Transform();
	transform->Set_Position(transform->Get_Position() + m_BodyShift);
	transform->Update_WorldMatrix();
	m_BodyShift = Vector3::Zero;
	Set_ChildAt(L"Em8000Body", Vector3::Zero);
	Play(ANIM::IDLE, true, 0.f);
	Change_State(STATE::IDLE);
}

void StateEm8000_Intro::Place_Camera() const
{
	Vector3 head = m_Landing + Vector3{ 0.f, 5.f, 0.f };
	if (const Shared<Em8000Body> body = m_Body.lock(); body && m_BodyBoneIndex != -1)
		head = (body->Get_ModelComponent()->Get_BoneMatrix(m_BodyBoneIndex) * body->Get_CombinedWorldMatrix()).Translation();
	m_Cutscene.Place_Camera(Shots, size(Shots), m_Time, EndTime, head);
}

void StateEm8000_Intro::Set_ChildAt(const wstring& child, const Vector3& worldOffset) const
{
	const Shared<Em8000> owner = m_Owner.lock();
	const Shared<GameObject> object = owner ? owner->Find_Child(child) : nullptr;
	if (!object) return;
	Quaternion inverse{};
	owner->Get_Transform()->Get_Quaternion().Inverse(inverse);
	object->Get_Transform()->Set_LocalPosition(Vector3::Transform(worldOffset, inverse));
}

Shared<StateEm8000_Intro> StateEm8000_Intro::Create(const wstring& tag, const Shared<Em8000>& owner)
{
	auto instance = make_shared<StateEm8000_Intro>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : StateEm8000_Intro");
		return nullptr;
	}

	return instance;
}
