#include "pch.h"
#include "State2B_Evade.h"

#include <Game.h>
#include <SpdLogger.h>

#include "Pl0000.h"
#include "Pl0000Body.h"
#include "Pl0000EvadeGhost.h"
#include "LaserEffect.h"
#include "WP0070Body.h"
#include "WP0220Body.h"
#include "Pl0000Input.h"
#include "Pl0000StateMachine.h"
#include "Model.h"
#include "Pl0000Movement.h"
#include "Random_Helper.h"

State2B_Evade::State2B_Evade(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{tag, owner}
{
}

HRESULT State2B_Evade::Initialize()
{
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::STAND_TO_DASH_F));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::STAND_TO_DASH_B));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::STAND_TO_DASH_R));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::STAND_TO_DASH_L));

	m_DashAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_F));
	m_DashAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_B));
	m_DashAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_R));
	m_DashAnim.emplace(ETOI(Pl0000::PL0000_STATE::DASH_L));

	m_EvadeAnim.emplace(ETOI(Pl0000::PL0000_STATE::EVADE_FRONT));
	m_EvadeAnim.emplace(ETOI(Pl0000::PL0000_STATE::EVADE_BACKWARD));
	m_EvadeAnim.emplace(ETOI(Pl0000::PL0000_STATE::EVADE_RIGHT));
	m_EvadeAnim.emplace(ETOI(Pl0000::PL0000_STATE::EVADE_LEFT));

	return State2B::Initialize();
}

Bool State2B_Evade::StateEnterInvoke()
{
	auto pl0000 = m_Body.lock();
	auto input = m_Input.lock();
	
	m_OriginalFov = m_MainCamera.lock()->Get_FovY();
	m_EvadePhase = EVADE_PHASE::ENTER;
	m_EvadeStreaks.reset();
	m_EvadeGhosts.clear();
	m_GhostDelay = 0.f;
	m_StreakSpawned = false;
	m_CrossSpawned = false;
	m_PerfectEvade = false;
	PlayEvadeSFX(m_EvadePhase);

	auto movement = m_Movement.lock();
	m_InAir = !movement->Is_Grounded();
	if (m_InAir)
	{
		m_AirDashDirection = Calculate_Direction();
		m_AirDashRemaining = AirDashTime;
		m_SavedGravity = movement->Get_Gravity();
		movement->Set_Gravity(0.f);
		movement->Reset_VerticalVelocity();
	}

	if (input->Is_KeyPress(UBYTE(DIKEYBOARD_W))) {
		m_DashDir = DASH_DIR::FRONT;
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::STAND_TO_DASH_F), 0.15f, false);
	}
	else if (input->Is_KeyPress(UBYTE(DIKEYBOARD_S))) {
		m_DashDir = DASH_DIR::BACK;
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::STAND_TO_DASH_B), 0.15f, false);
	}
	else if (input->Is_KeyPress(UBYTE(DIKEYBOARD_D))) {
		m_DashDir = DASH_DIR::RIGHT;
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::STAND_TO_DASH_R), 0.15f, false);
	}
	else if (input->Is_KeyPress(UBYTE(DIKEYBOARD_A))) {
		m_DashDir = DASH_DIR::LEFT;
		pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::STAND_TO_DASH_L), 0.15f, false);
	}
	else {
		LOG_ERROR(L"Wrong Input At DashState Enter");
		return false;
	}
	/* (이전 시간에 구성한 MovementData 방향 연산 및 주입은 필요시 Update나 여기서 1회 수행) */

	return true;

}

void State2B_Evade::Update(Float timeDelta)
{
	auto pl0000 = m_Body.lock();
	auto input = m_Input.lock();
	uint32 curIndex = pl0000->Get_CurrentAnimationIndex();
	Bool isAnimFinished = pl0000->Is_AnimationFinished();
	Float dashProgress = pl0000->Get_AnimationProgress();
	if (m_PerfectEvade && !m_CrossSpawned)
	{
		m_GhostDelay += timeDelta;
		if (!m_StreakSpawned && m_GhostDelay >= 0.05f)
		{
			m_StreakSpawned = true;
			m_EvadeStreaks = pl0000->Spawn_EvadeStreaks();
			if (auto streaks = m_EvadeStreaks.lock())
				streaks->Strengthen_PerfectEvade();
		}
		if (m_GhostDelay >= 0.07f)
		{
			m_CrossSpawned = true;
			for (const auto& ghost : pl0000->Spawn_EvadeCross(0.55f, n_GhostColor))
			{
				if (ghost && m_PerfectEvade)
					ghost->Strengthen_PerfectEvade();
				m_EvadeGhosts.push_back(ghost);
			}
		}
	}

	if (m_EvadePhase == EVADE_PHASE::EVADE)
	{
		Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
		moveData.direction = Calculate_Direction();
		moveData.isMove = (moveData.direction.LengthSquared() > 0.f);
		moveData.useRootMotionDir = false;
		m_Movement.lock()->Set_MovementData(moveData);
		if (dashProgress <= 0.5f)
			return;

		m_Owner.lock()->Set_Invincible(false);
		if (input->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)))
		{
			m_States.lock()->Change_State(Pl0000::PL0000_STATE::JUMP);
			return;
		}
		if (input->Is_MousePress(DIMB::LBUTTON) || input->Is_MousePress(DIMB::RBUTTON))
		{
			if (auto target = m_Owner.lock()->Get_LockOnTarget().lock())
			{
				if (!target->Is_Destroy())
				{
					auto targetTransform = target->Get_Component<Transform>();
					Vector3 myPos = m_Owner.lock()->Get_Transform()->Get_Position();
					Vector3 targetPos = targetTransform->Get_Position();
					targetPos.y = myPos.y;
					
					m_Owner.lock()->Get_Transform()->LookAt(targetPos);
				}
			}

			m_States.lock()->Change_State(Pl0000::PL0000_STATE::ATTACK_GROUND);
			return;
		}
		if (input->Is_WASD_DoubleClickedHold(0.f))
		{
			m_States.lock()->Change_State(Pl0000::PL0000_STATE::SPRINT);
			return;
		}
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
		return;
	}

	// DASH 

	Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
	moveData.direction = m_InAir ? m_AirDashDirection : Calculate_Direction();
	moveData.isMove = !m_InAir && (moveData.direction.LengthSquared() > 0.f);
	moveData.isJump = m_InAir;
	m_Movement.lock()->Reduce_RootMotion(0.25f);

	if (m_DashDir != DASH_DIR::FRONT)
	{
		Vector3 camLook = m_MainCamera.lock()->Get_Transform()->Get_Look();
		camLook.y = 0.f; // 위아래로 기울지 않도록 XZ평면으로 납작하게 투영
		camLook.Normalize();
		moveData.lookDirection = camLook;
	}
	
	moveData.useRootMotionDir = false;
	m_Movement.lock()->Set_MovementData(moveData);

	if (m_InAir)
	{
		// The air keeps no root motion, so the burst is moved here.
		m_Movement.lock()->Add_Correction(m_AirDashDirection * (AirDashSpeed * timeDelta));
		m_AirDashRemaining -= timeDelta;
		if (m_Movement.lock()->Is_Grounded())
		{
			m_EvadePhase = EVADE_PHASE::END;
			m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
			return;
		}
		if (m_AirDashRemaining <= 0.f)
		{
			m_EvadePhase = EVADE_PHASE::END;
			m_States.lock()->Change_State(Pl0000::PL0000_STATE::JUMP);
			return;
		}
	}
	else if (input->Is_KeyDown(UBYTE(DIKEYBOARD_SPACE)))
	{
		GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_7);
		m_EvadePhase = EVADE_PHASE::END;
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::JUMP);
		return;
	}
	else if (input->Is_WASD_NoneOrUp())
	{
		GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_7);
		m_EvadePhase = EVADE_PHASE::END;
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
		return;
	}

	if (m_EnterAnim.contains(curIndex) && dashProgress >= 0.8f)
	{
		switch (m_DashDir) {
		case DASH_DIR::FRONT: 
			m_EvadePhase = EVADE_PHASE::DASH;
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_F), 0.5f, false); 
			break;
		case DASH_DIR::BACK:  
			m_EvadePhase = EVADE_PHASE::DASH;
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_B), 0.5f, false); 
			break;
		case DASH_DIR::RIGHT: 
			m_EvadePhase = EVADE_PHASE::DASH;
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_R), 0.5f, false); 
			break;
		case DASH_DIR::LEFT:  
			m_EvadePhase = EVADE_PHASE::DASH;
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::DASH_L), 0.5f, false); 
			break;
		case DASH_DIR::NONE:
			LOG_ERROR(L"Wrong Dash Direction");
			break;
		}
	}
	else if (m_DashAnim.contains(curIndex) && dashProgress >= 0.7f)
	{
		m_EvadePhase = EVADE_PHASE::END;
		if (!input->Is_WASD_NoneOrUp())
		{
			m_States.lock()->Change_State(Pl0000::PL0000_STATE::SPRINT);
			return;
		}
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
	}
}

void State2B_Evade::Late_Update(Float timeDelta)
{
	if (m_EvadePhase == EVADE_PHASE::EVADE)
		return;

	Float dashProgress = m_Body.lock()->Get_AnimationProgress();

	Float extraFovRadian = sinf(dashProgress * XM_PI) * XMConvertToRadians(8.f);
	m_MainCamera.lock()->Set_FovY(m_OriginalFov + extraFovRadian);
}

void State2B_Evade::StateExitInvoke()
{
	if (m_OriginalFov > 0.f)
	{
		m_MainCamera.lock()->Set_FovY(m_OriginalFov);
		m_OriginalFov = 0.f;
	}

	m_Movement.lock()->Reset_RootMotionStop();
	if (m_InAir)
	{
		m_Movement.lock()->Set_Gravity(m_SavedGravity);
		m_InAir = false;
	}
}

void State2B_Evade::PlayEvadeSFX(EVADE_PHASE evadePhase) const
{
	if (evadePhase == EVADE_PHASE::ENTER || evadePhase == EVADE_PHASE::DASH)
	{
		// Pl_Dash 프리셋이 변형 4개 중 하나를 랜덤으로 고른다.
		GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_7);
		GAME_INSTANCE->PlaySoundFXOnce(L"Pl_Dash", SOUNDCHANNEL::CHANNEL_7, 0.5f);
	}
	else if (evadePhase == EVADE_PHASE::EVADE)
	{
		GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_7);
		GAME_INSTANCE->PlaySoundFXOnce(L"Pl_Evade", SOUNDCHANNEL::CHANNEL_7, 0.5f);
	}
}

Bool State2B_Evade::TryEvade_FromDash()
{
	if (m_EvadePhase == EVADE_PHASE::EVADE || (m_InAir && m_PerfectEvade)) return true;

	auto pl0000 = m_Body.lock();
	Float progress = pl0000->Get_AnimationProgress();

	if (progress >= 0.75f)
		return false;

	// In the air the dash runs on (the evade clips step along the floor); the rest of the perfect dodge is the same.
	if (!m_InAir)
	{
		switch (m_DashDir)
		{
		case DASH_DIR::FRONT:
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::EVADE_FRONT), 0.1f, false);
			break;
		case DASH_DIR::BACK:
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::EVADE_BACKWARD), 0.1f, false);
			break;
		case DASH_DIR::RIGHT:
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::EVADE_RIGHT), 0.1f, false);
			break;
		case DASH_DIR::LEFT:
			pl0000->Set_Animation(ETOI(Pl0000::PL0000_STATE::EVADE_LEFT), 0.1f, false);
			break;
		default:
			return false;
		}
		m_EvadePhase = EVADE_PHASE::EVADE;
	}
	PlayEvadeSFX(EVADE_PHASE::EVADE);

	auto owner = m_Owner.lock();
	owner->Set_InvincibleTime(PerfectEvadeInvincibleTime);
	owner->Trigger_GlobalLag(0.2f, 0.75f);
	// The colour split alone, weaker than a hit's glitch. Tuned on screen.
	GAME_INSTANCE->Start_ScreenGlitch(0.3f, 0.35f, false);

	m_PerfectEvade = true;
	m_GhostDelay = 0.f;
	if (auto laserEffect = static_pointer_cast<LaserEffect>(owner->Find_Child(L"LaserEffect")))
		laserEffect->PlayEvadeBurst(owner->Get_Transform()->Get_Position() + Vector3::Up * 0.85f);
	return true; 
}

Shared<State2B_Evade> State2B_Evade::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_Evade>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		MSG_BOX("Failed to Created : State2B_Dash");
		return nullptr;
	}

	return instance;
}
