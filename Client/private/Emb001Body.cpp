#include "pch.h"
#include "Emb001Body.h"

#include <Game.h>
#include <Model.h>
#include <SoundEmitter.h>

#include "Em0000.h"
#include "MonsterAOE.h"

Emb001Body::Emb001Body() : Em0000Body{} {}
Emb001Body::Emb001Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Em0000Body{ device, context } {}
Emb001Body::Emb001Body(const Emb001Body& rhs)
	: Em0000Body{ rhs } {}

void Emb001Body::Ready_MoveNotify()
{
	using em0000State = Em0000::EM0000_STATE;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	// The shared clips' seqs (em0000_<mot>_0_seq) name em0000.bnk's light steps; the rabbit takes the heavy ones of its
	// own bank (emb001.bnk, as in its wake clip) on the same frames.
	auto playSound = [this](const tChar* tag) { m_SoundEmitter->Play(tag); };
	m_Model->Add_AnimNotify(ETOI(em0000State::WALK_F), {
		notify{ L"emb001_step_R", 14.f, [playSound]() { playSound(L"emb001_step_R"); } },
		notify{ L"emb001_step_L", 44.f, [playSound]() { playSound(L"emb001_step_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::DASH), {
		notify{ L"emb001_jump", 1.f, [playSound]() { playSound(L"emb001_jump"); } },
		notify{ L"emb001_jump_land", 42.f, [playSound]() { playSound(L"emb001_jump_land"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::ESCAPE_BACK), {
		notify{ L"emb001_jump", 0.f, [playSound]() { playSound(L"emb001_jump"); } },
		notify{ L"emb001_jump_land", 21.f, [playSound]() { playSound(L"emb001_jump_land"); } },
	});
}

void Emb001Body::Ready_AttackNotify()
{
	using em0000State = Em0000::EM0000_STATE;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	// 원작 emb001 변형 SeTrack(em0000_<mot>_1_seq)의 사운드 이벤트(emb001.bnk). Stop_ 이벤트는 뺐다.
	auto playSound = [this](const tChar* tag) { m_SoundEmitter->Play(tag); };
	auto telegraph = [this]() { Telegraph(); };
	auto armsOn = [this]() { m_LeftArm.lock()->Active_Attack(); m_RightArm.lock()->Active_Attack(); };
	auto armsOff = [this]() { m_LeftArm.lock()->DeActive_Attack(); m_RightArm.lock()->DeActive_Attack(); };
	auto torsoOn = [this]() { m_Torso.lock()->Active_Attack(); };
	auto torsoOff = [this]() { m_Torso.lock()->DeActive_Attack(); };

	m_Model->Add_AnimNotify(ETOI(em0000State::WAKE), {
		notify{ L"emb001_arm_move_01", 0.f, [playSound]() { playSound(L"emb001_arm_move_01"); } },
		notify{ L"emb001_battle_start_add_02", 0.f, [playSound]() { playSound(L"emb001_battle_start_add_02"); } },
		notify{ L"emb001_battle_start_add_01", 26.f, [playSound]() { playSound(L"emb001_battle_start_add_01"); } },
		notify{ L"emb001_battle_start", 51.f, [playSound]() { playSound(L"emb001_battle_start"); } },
		notify{ L"emb001_step_R", 76.f, [playSound]() { playSound(L"emb001_step_R"); } },
		notify{ L"emb001_step_L", 106.f, [playSound]() { playSound(L"emb001_step_L"); } },
	});

	// Strike windows are the original AttackTrack frames of the variant clips (headbutt 63-70, punch 72-76).
	m_Model->Add_AnimNotify(ETOI(em0000State::PUNCH), {
		notify{ L"emb001_body_move", 0.f, [playSound]() { playSound(L"emb001_body_move"); } },
		notify{ L"emb001_headbutt_wsh", 47.f, [playSound]() { playSound(L"emb001_headbutt_wsh"); } },
		notify{ L"emb001_jump", 55.f, [playSound]() { playSound(L"emb001_jump"); } },
		notify{ L"emb001_jump_land", 74.f, [playSound]() { playSound(L"emb001_jump_land"); } },
		notify{ L"Telegraph", 0.f, telegraph },
		notify{ L"Headbutt", 63.f, 70.f, torsoOn, torsoOff },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::SWING_LUNGE), {
		notify{ L"emb001_body_move", 0.f, [playSound]() { playSound(L"emb001_body_move"); } },
		notify{ L"emb001_straight_punch_wsh_01", 8.f, [playSound]() { playSound(L"emb001_straight_punch_wsh_01"); } },
		notify{ L"emb001_straight_punch_wsh_02", 55.f, [playSound]() { playSound(L"emb001_straight_punch_wsh_02"); } },
		notify{ L"Telegraph", 0.f, telegraph },
		notify{ L"Punch", 72.f, 76.f, armsOn, armsOff },
	});

	// The trumpet: raised with the attack's telegraph and lowered after the blow (Emb001 fires the orbs while it plays 05b1).
	m_Model->Add_AnimNotify(ETOI(em0000State::TRUMPET_START), {
		notify{ L"emb001_arm_move_02", 0.f, [playSound]() { playSound(L"emb001_arm_move_02"); } },
		notify{ L"Telegraph", 0.f, telegraph },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::TRUMPET_END), {
		notify{ L"emb001_arm_move_01", 31.f, [playSound]() { playSound(L"emb001_arm_move_01"); } },
	});
}

Shared<Emb001Body> Emb001Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Emb001Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Emb001Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Emb001Body::Clone(void* arg)
{
	auto instance = make_shared<Emb001Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Emb001Body");
		return nullptr;
	}

	return instance;
}
