#include "pch.h"
#include "State2B_AttackGround.h"

#include <Game.h>
#include <SpdLogger.h>

#include "Pl0000.h"
#include "Pl0000StateMachine.h"
#include "Pl0000Movement.h"
#include "WP0070Body.h"
#include "WP0220Body.h"

State2B_AttackGround::State2B_AttackGround(const wstring& tag, const Shared<Pl0000>& owner)
	: State2B{tag, owner}
{
}

HRESULT State2B_AttackGround::Initialize()
{
	Set_AnimationExitProgress();

	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND_RUN));
	m_EnterAnim.emplace(ETOI(Pl0000::PL0000_STATE::HEAVY_GROUND_SPRINT));

	return State2B::Initialize();
}

Bool State2B_AttackGround::StateEnterInvoke()
{
	m_AirWeapon = AIR_WEAPON::NONE;
	m_Movement.lock()->Reset_RootMotionStop();

	Pl0000Movement::PL0000_MOVEMENT_DATA moveData{};
	moveData.useRootMotionDir = true;
	m_Movement.lock()->Set_MovementData(moveData);

	auto pl0000 = m_Owner.lock();
	auto pl0000Body = m_Body.lock();
	auto lightWeapon = m_LightWeapon.lock();
	auto heavyWeapon = m_HeavyWeapon.lock();
	auto prevState = m_States.lock()->Get_CurPl0000State();

	m_ComboDelta = 0.f;
	m_ComboStep = 0;
	Bool clickLeft = m_Input.lock()->Is_MousePress(DIMB::LBUTTON) || m_Input.lock()->Is_MouseDown(DIMB::LBUTTON);
	Face_ComboDirection();

	switch (prevState)
	{
	case Pl0000::PL0000_STATE::IDLE: case Pl0000::PL0000_STATE::ATTACK_AIR: case Pl0000::PL0000_STATE::HIT:
	case Pl0000::PL0000_STATE::LEDGE:
		if (clickLeft)
		{
			pl0000->Draw_LightWeapon();
			pl0000->Sheathe_HeavyWeapon();
			pl0000Body->Set_Animation(LIGHT_BODY[m_ComboStep], 0.05f, false);
			lightWeapon->Set_Animation(LIGHT_WP[m_ComboStep], 0.f, false);
			m_LastOrderedAnimIndex = LIGHT_BODY[m_ComboStep];
			
			++m_ComboStep;
			m_PrevComboType = COMBO_TYPE::LIGHT;
		}
		else
		{
			pl0000->Sheathe_LightWeapon();
			pl0000->Draw_HeavyWeapon();
			pl0000Body->Set_Animation(m_HeavyBody[m_ComboStep], 0.05f, false);
			heavyWeapon->Set_Animation(m_HeavyWP[m_ComboStep], 0.f, false);
			m_LastOrderedAnimIndex = m_HeavyBody[m_ComboStep];
			
			++m_ComboStep;
			m_PrevComboType = COMBO_TYPE::HEAVY;
		}
		return true;
	case Pl0000::PL0000_STATE::RUN: case Pl0000::PL0000_STATE::SPRINT: case Pl0000::PL0000_STATE::EVADE:
		if (clickLeft)
		{
			pl0000->Draw_LightWeapon();
			pl0000->Sheathe_HeavyWeapon();
			pl0000Body->Set_Animation(ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND_RUN), 0.05f, false);
			lightWeapon->Set_Animation(ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND_RUN), 0.f, false);
			m_LastOrderedAnimIndex = ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND_RUN);

			++m_ComboStep;
			m_PrevComboType = COMBO_TYPE::LIGHT;
		}
		else
		{
			pl0000->Sheathe_LightWeapon();
			pl0000->Draw_HeavyWeapon();
			pl0000Body->Set_Animation(m_HeavyBody[m_ComboStep], 0.05f, false);
			heavyWeapon->Set_Animation(m_HeavyWP[m_ComboStep], 0.f, false);
			m_LastOrderedAnimIndex = m_HeavyBody[m_ComboStep];

			++m_ComboStep;
			m_PrevComboType = COMBO_TYPE::HEAVY;
		}
		return true;
	default:
		LOG_ERROR(L"[ENTER ATTACK GROUND] : No Enter state PrevIndex {} ", ETOI(prevState));
		return false;
	}
}

void State2B_AttackGround::Update(Float timeDelta)
{
	auto pl0000 = m_Body.lock();
	auto input = m_Input.lock();
	Bool isAnimFinished = pl0000->Is_AnimationFinished();
	Float progress = pl0000->Get_AnimationProgress();

	uint32 animIndex = pl0000->Get_CurrentAnimationIndex();
	if (!m_Movement.lock()->Is_Grounded() &&
		animIndex == ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND7) &&
		(input->Is_MousePress(DIMB::LBUTTON) || input->Is_MousePress(DIMB::RBUTTON)))
	{
		m_AirWeapon = input->Is_MousePress(DIMB::LBUTTON) ? AIR_WEAPON::LIGHT : AIR_WEAPON::HEAVY;
		if (m_AirWeapon == AIR_WEAPON::LIGHT) m_LightWeapon.lock()->Recall_ThrownWeapon();
		if (!m_States.lock()->Change_State(Pl0000::PL0000_STATE::ATTACK_AIR))
			m_AirWeapon = AIR_WEAPON::NONE;
		return;
	}

	if (!input->Is_MousePress(DIMB::LBUTTON))
		m_HoldReleased = true;

	// 항상 ATTACK보다 DASH 판정해서 EVADE로 빼기
	if (input->Is_WASD_DoubleClick())
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::EVADE);
		return;
	}

	// Each new swing turns toward the lock-on target (or the WASD input), as the air combo's do.
	const uint32 orderedAnimIndex = m_LastOrderedAnimIndex;
	Execute_Attack();
	if (m_LastOrderedAnimIndex != orderedAnimIndex)
		Face_ComboDirection();
	// Execute_Attack can replace light 6 with 7 on this frame, so inspect the current clip.
	animIndex = pl0000->Get_CurrentAnimationIndex();
	if (!m_Movement.lock()->Is_Grounded() &&
		(animIndex == ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND6) ||
		 animIndex == ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND7)))
	{
		if (pl0000->Is_AnimationFinished())
		{
			m_AirWeapon = input->Is_MousePress(DIMB::LBUTTON) ? AIR_WEAPON::LIGHT :
				input->Is_MousePress(DIMB::RBUTTON) ? AIR_WEAPON::HEAVY : AIR_WEAPON::NONE;
			if (!m_States.lock()->Change_State(Pl0000::PL0000_STATE::ATTACK_AIR))
				m_AirWeapon = AIR_WEAPON::NONE;
		}
		return;
	}

	// The left button still held after the hold attack is the hold's own, so it does not keep the player in the clip.
	const Bool holdClip = animIndex == ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND_HOLD);
	if ((!holdClip && (input->Is_MousePress(DIMB::LBUTTON) || input->Is_MouseHold(DIMB::LBUTTON, 0.2f))) ||
		input->Is_MousePress(DIMB::RBUTTON) ||
		input->Is_MouseHold(DIMB::RBUTTON, 0.2f) ||
		m_IsHeavyCharge)
	{
		return; // 이하 RUN 등 탈출 로직 완전 무시
	}

	if (m_CanExitProgress[animIndex] >= pl0000->Get_AnimationProgress()) return;

	if (input->Is_WASD_Press())
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::RUN);
		return;
	}

	if (holdClip && pl0000->Get_AnimationProgress() >= n_HoldEnd)
	{
		m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
		return;
	}
	if (false == isAnimFinished) return;
	m_States.lock()->Change_State(Pl0000::PL0000_STATE::IDLE);
}

void State2B_AttackGround::Late_Update(Float timeDelta)
{
	m_ComboDelta += timeDelta;
	if (m_ComboDelta >= n_ComboDelta &&
		(m_Movement.lock()->Is_Grounded() || m_ComboStep < 6))
	{
		m_ComboDelta = 0.f;
		m_ComboStep = 0;
	}

	if (m_IsHeavyCharge)
	{
		m_HeavyChargeDelta += timeDelta;

		if (!m_IsChargeEnd && m_HeavyChargeDelta >= n_ChargeDelta)
		{
			m_IsChargeEnd = true;
			m_Body.lock()->Spawn_Ghost(0.5f, Vector4{ 1.f, 1.f, 1.f, 1.f });
			GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_8);
			GAME_INSTANCE->PlaySoundFXOnce(L"Pl_Charge_End", SOUNDCHANNEL::CHANNEL_8, 0.5f);
			// 풀차지 뒤 버튼을 떼거나 상태를 벗어날 때까지 차지 유지음을 반복한다.
			GAME_INSTANCE->PlaySoundLoopSection(L"Pl_Charge_Loop", SOUNDCHANNEL::CHANNEL_11, 0.5f, 0, UINT32_MAX, true);
		}
	}
}

void State2B_AttackGround::StateExitInvoke()
{
	GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_11);   // 차지 중 피격·회피로 빠져도 유지음이 남지 않게
	m_LightWeapon.lock()->End_LaunchAttack();
	if (m_AirWeapon != AIR_WEAPON::LIGHT) m_Owner.lock()->Sheathe_LightWeapon();
	if (m_AirWeapon != AIR_WEAPON::HEAVY) m_Owner.lock()->Sheathe_HeavyWeapon();
	m_AirWeapon = AIR_WEAPON::NONE;
		
	m_Movement.lock()->Reset_RootMotionStop();
}

void State2B_AttackGround::Execute_Attack()
{
	auto pl0000 = m_Owner.lock();
	auto pl0000Body = m_Body.lock();
	auto input = m_Input.lock();
	auto lightWeapon = m_LightWeapon.lock();
	auto heavyWeapon = m_HeavyWeapon.lock();
	uint32 actualAnimIndex = pl0000Body->Get_CurrentAnimationIndex();
	Float blendDuration = 0.15f;

	if (m_IsHeavyCharge)
	{
		if (input->Is_MouseUp(DIMB::RBUTTON) || !input->Is_MousePress(DIMB::RBUTTON))
		{
			if (m_HeavyChargeDelta >= n_ChargeDelta) // 풀차지
			{
				m_LastOrderedAnimIndex = ETOI(Pl0000::PL0000_STATE::HEAVY_GROUND_HOLD_FULL);
				pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
				heavyWeapon->Set_Animation(ETOI(WP0220Body::WP0220_STATE::HEAVY_GROUND_HOLD_FULL), blendDuration, false);
			}
			else // 미완성 차지
			{
				m_LastOrderedAnimIndex = ETOI(Pl0000::PL0000_STATE::HEAVY_GROUND_HOLD_UNFULL);
				pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
				heavyWeapon->Set_Animation(ETOI(WP0220Body::WP0220_STATE::HEAVY_GROUND_HOLD_UNFULL), blendDuration, false);
			}

			GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_11);
			m_ComboStep = 0;
			m_PrevComboType = COMBO_TYPE::HEAVY;
			m_IsHeavyCharge = false;
			m_IsChargeEnd = false;
		}
		return; // 아직 차징 중이면 다른 입력은 컷
	}

	Bool holdHeavy = input->Is_MouseHold(DIMB::RBUTTON, 0.2f);
	if (holdHeavy && m_PrevComboType == COMBO_TYPE::HEAVY && (m_ComboStep <= 1))
	{
		if (actualAnimIndex != ETOI(Pl0000::PL0000_STATE::HEAVY_GROUND_HOLD_CYCLE))
		{
			GAME_INSTANCE->PlaySoundFXOnce(L"Pl_Charge_Start", SOUNDCHANNEL::CHANNEL_8, 0.5f);

			m_IsHeavyCharge = true;
			m_HeavyChargeDelta = 0.f;
			m_LastOrderedAnimIndex = ETOI(Pl0000::PL0000_STATE::HEAVY_GROUND_HOLD_CYCLE);
			pl0000Body->Set_Animation(m_LastOrderedAnimIndex, 0.2f, true);
			pl0000->Sheathe_LightWeapon();
			pl0000->Draw_HeavyWeapon();
			heavyWeapon->Set_Animation(ETOI(WP0220Body::WP0220_STATE::HEAVY_GROUND_HOLD_CYCLE), 0.3f, true);
			m_ComboStep = 0;
			m_PrevComboType = COMBO_TYPE::HEAVY;
			return;
		}
	}

	if (m_ComboStep > 0)
	{
		// 내가 틀라고 명령한 애니메이션으로 아직 모델이 완전히 전환되지 않았다면? 클릭 판정을 아예 보류.
		if (actualAnimIndex != m_LastOrderedAnimIndex) return;
		// 전환이 완료되었다면 진행률(isExitProgress) 검사 수행
		Bool isExitProgress = pl0000Body->Get_AnimationProgress() >= m_CanComboProgress[actualAnimIndex];
		if (false == isExitProgress) return;
	}

	Bool tryLight = input->Is_MousePress(DIMB::LBUTTON) || input->Is_MouseHold(DIMB::LBUTTON, 0.05f);
	Bool holdLight = input->Is_MouseHold(DIMB::LBUTTON, 0.1f);
	Bool tryHeavy = input->Is_MousePress(DIMB::RBUTTON) || input->Is_MouseHold(DIMB::RBUTTON, 0.05f);

	if (tryLight)
	{
		m_ComboDelta = 0.f;
		// A new click after the hold attack (its button let go) starts the light combo over from the first hit.
		if (m_ComboStep == 7 && m_HoldReleased && actualAnimIndex == ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND_HOLD))
			m_ComboStep = 0;
		switch (m_PrevComboType) {
		case COMBO_TYPE::LIGHT:
			
			if (holdLight && m_PrevComboType == COMBO_TYPE::LIGHT && (m_ComboStep >= 1 && m_ComboStep <= 5))
			{
				if (actualAnimIndex != ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND_HOLD))
				{
					m_LastOrderedAnimIndex = ETOI(Pl0000::PL0000_STATE::LIGHT_GROUND_HOLD);
					pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
					lightWeapon->Set_Animation(ETOI(WP0070Body::WP0070_STATE::LIGHT_GROUND_HOLD), blendDuration, false);
					pl0000->Draw_LightWeapon();
					// Ends the light chain like the 7th hit. Step 0 would skip the progress gate above and the still-held
					// button would restart LIGHT_GROUND1 on the next frame, cutting the hold attack off.
					m_ComboStep = 7;
					m_PrevComboType = COMBO_TYPE::LIGHT;
					m_HoldReleased = false;
					break; // 홀드 했으면 콤보 흐름 강제 중단
				}
			}
			else if (m_ComboStep == 0) // 루트 진입 (0타)
			{
				m_LastOrderedAnimIndex = LIGHT_BODY[0];
				pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
				if (lightWeapon->Is_Sheathing()) lightWeapon->DrawWP0070();
				lightWeapon->Set_Animation(LIGHT_WP[0], blendDuration, false);
				pl0000->Sheathe_HeavyWeapon();

				m_ComboStep = 1;
			}
			else if (m_ComboStep < 7) // 1~6타
			{
				if (m_ComboStep == 6) lightWeapon->End_LaunchAttack();
				if (m_ComboStep == 5)
				{
					auto movement = m_Movement.lock();
					auto moveData = movement->Get_MovementData();
					moveData.isAttack = true;
					movement->Set_MovementData(moveData);
				}
				m_LastOrderedAnimIndex = LIGHT_BODY[m_ComboStep];
				pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
				lightWeapon->Set_Animation(LIGHT_WP[m_ComboStep], blendDuration, false);
				pl0000->Draw_LightWeapon();
				if (m_ComboStep == 5) lightWeapon->Begin_LaunchAttack();

				m_ComboStep++;
			}
			break;
		default:
			// 강공 및 기타 상태에서 좌클릭 시 리셋 안정성 보장
			m_ComboStep = 0;
			m_LastOrderedAnimIndex = LIGHT_BODY[m_ComboStep];
			pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
			pl0000->Draw_LightWeapon();
			lightWeapon->Set_Animation(LIGHT_WP[m_ComboStep], blendDuration, false);
			pl0000->Sheathe_HeavyWeapon();

			m_PrevComboType = COMBO_TYPE::LIGHT;
			m_ComboStep = 1;
			break;
		}
	}
	else if (tryHeavy)
	{
		m_ComboDelta = 0.f;
		switch (m_PrevComboType) {
		case COMBO_TYPE::HEAVY:
			if (m_ComboStep == 0) // 루트 진입 (0타)
			{
				m_LastOrderedAnimIndex = m_HeavyBody[0];
				pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
				if (heavyWeapon->Is_Sheathing()) pl0000->Draw_HeavyWeapon();
				pl0000->Sheathe_LightWeapon();
				heavyWeapon->Set_Animation(m_HeavyWP[0], blendDuration, false);
	
				m_ComboStep = 1;
			}
			else if (m_ComboStep < m_HeavyBody.size()) // 강공 2타부터
			{
				m_LastOrderedAnimIndex = m_HeavyBody[m_ComboStep];
				pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
				pl0000->Sheathe_LightWeapon();
				pl0000->Draw_HeavyWeapon();
				heavyWeapon->Set_Animation(m_HeavyWP[m_ComboStep], blendDuration, false);

				m_ComboStep++;
			}
			break;
		case COMBO_TYPE::LIGHT:
			if (m_ComboStep != 0 && m_ComboStep < 7)
			{
				m_LastOrderedAnimIndex = ETOI(Pl0000::PL0000_STATE::LIGHT_HEAVY_COMBO);
				pl0000Body->Set_Animation(m_LastOrderedAnimIndex, 0.1f, false);
				pl0000->Draw_LightWeapon();
				lightWeapon->Set_Animation(ETOI(WP0070Body::WP0070_STATE::LIGHT_COMBO), 0.1f, false);
				pl0000->Draw_HeavyWeapon();
				heavyWeapon->Set_Animation(ETOI(WP0220Body::WP0220_STATE::HEAVY_COMBO), 0.1f, false);
	
				m_ComboStep = -1;
				m_PrevComboType = COMBO_TYPE::LIGHT_HEAVY;
			}
			else
			{
				m_ComboStep = 0;
				m_LastOrderedAnimIndex = m_HeavyBody[m_ComboStep];
				pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
				pl0000->Sheathe_LightWeapon();
				heavyWeapon->Set_Animation(m_HeavyWP[m_ComboStep], blendDuration, false);
				pl0000->Draw_HeavyWeapon();

				m_ComboStep = 1;
				m_PrevComboType = COMBO_TYPE::HEAVY;
			}
			break;
		case COMBO_TYPE::LIGHT_HEAVY:
		default:
			// 교차 콤보 직후 우클 누를 때 정상적인 첫 타 복귀
			m_ComboStep = 0;
			m_LastOrderedAnimIndex = m_HeavyBody[m_ComboStep];
			pl0000Body->Set_Animation(m_LastOrderedAnimIndex, blendDuration, false);
			pl0000->Sheathe_LightWeapon();
			heavyWeapon->Set_Animation(m_HeavyWP[m_ComboStep], blendDuration, false);
			pl0000->Draw_HeavyWeapon();

			m_ComboStep = 1;
			m_PrevComboType = COMBO_TYPE::HEAVY;
			break;
		}
	}

}

void State2B_AttackGround::Set_AnimationExitProgress()
{
	using pl = Pl0000::PL0000_STATE;
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND_RUN), 0.1f);
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND1), 0.1f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND1), 0.1f);
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND2), 0.075f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND2), 0.075f);
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND3), 0.075f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND3), 0.1f);
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND4), 0.1f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND4), 0.1f);
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND5), 0.1f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND5), 0.1f);
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND6), 0.1f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND6), 0.1f);
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND7), 0.5f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND7), 0.4f);

	// The hold attack's last hit ends at frame 60 of 331: the next attack or a move can follow from frame 66.
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND_HOLD), 0.2f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND_HOLD), 0.2f);
	m_CanComboProgress.emplace(ETOI(pl::LIGHT_GROUND_RUN), 0.25f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_GROUND_RUN), 0.15f);

	m_CanComboProgress.emplace(ETOI(pl::HEAVY_GROUND1), 0.15f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_GROUND1), 0.2f);
	m_CanComboProgress.emplace(ETOI(pl::HEAVY_GROUND2), 0.15f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_GROUND2), 0.2f);
	m_CanComboProgress.emplace(ETOI(pl::HEAVY_GROUND3), 0.3f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_GROUND3), 0.3f);

	m_CanComboProgress.emplace(ETOI(pl::HEAVY_GROUND_HOLD_CYCLE), 0.1f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_GROUND_HOLD_CYCLE), 0.1f);
	m_CanComboProgress.emplace(ETOI(pl::HEAVY_GROUND_HOLD_UNFULL), 0.5f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_GROUND_HOLD_UNFULL), 0.25f);
	m_CanComboProgress.emplace(ETOI(pl::HEAVY_GROUND_HOLD_NO_CONTACT), 0.5f); m_CanExitProgress.emplace(ETOI(pl::HEAVY_GROUND_HOLD_NO_CONTACT), 0.25f);

	m_CanComboProgress.emplace(ETOI(pl::LIGHT_HEAVY_COMBO), 0.5f); m_CanExitProgress.emplace(ETOI(pl::LIGHT_HEAVY_COMBO), 0.4f);
}

Shared<State2B_AttackGround> State2B_AttackGround::Create(const wstring& tag, const Shared<Pl0000>& owner)
{
	auto instance = make_shared<State2B_AttackGround>(tag, owner);

	if (FAILED(instance->Initialize()))
	{
		LOG_ERROR(L"Failed to Create State2B_AttackGround");
		return nullptr;
	}

	return instance;
}
