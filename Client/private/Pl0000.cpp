#include "pch.h"
#include "Pl0000.h"

#include <Game.h>
#include <SpdLogger.h>
#include <SphereCollider.h>

#include "Bullet.h"
#include "FireFlashEffect.h"
#include "LaserEffect.h"
#include "Monster.h"
#include "Pl0000Body.h"
#include "Pl0000HpBar.h"
#include "Pl0000Movement.h"
#include "Pl0000StateMachine.h"
#include "PodLaser.h"
#include "SheathWP0070Body.h"
#include "SheathWP0220Body.h"
#include "SwordWave.h"
#include "State2B_AttackAir.h"
#include "State2B_AttackGround.h"
#include "WP3000Body.h"
#include "WeaponHalo.h"

#include "StateMachine.h"
#include "State2B_Idle.h"
#include "State2B_Jump.h"
#include "State2B_Run.h"
#include "State2B_Sprint.h"
#include "State2B_Walk.h"
#include "State2B_Evade.h"
#include "State2B_Hit.h"
#include "State2B_PodGlide.h"
#include "State2B_PodAttack.h"
#include "State2B_Ledge.h"
#include "StateA2_AttackAir.h"
#include "StateA2_AttackGround.h"

namespace Client {

namespace
{
	constexpr Float PodLaserCooldownTime = 10.f;
	// A target picked with G lets go only past this distance (or when it dies), so it holds when the player turns away.
	constexpr Float ManualLockRange = 30.f;	// 1.5x the original 20 m, with the wider G pick range
	// Taking or dropping that lock clicks like the original's UI (core_target_lockon/lockoff).
	constexpr SOUNDCHANNEL LockOnChannel = SOUNDCHANNEL::CHANNEL_2;
	constexpr Float LockOnVolume = 0.5f;
	// Taking a hit glitches the screen (Game::Start_ScreenGlitch), harder and longer for one that knocks 2B back.
	// Tuned on screen.
	constexpr Float HitGlitch = 0.65f;
	constexpr Float HitGlitchTime = 0.25f;
	constexpr Float HeavyHitGlitch = 1.15f;
	constexpr Float HeavyHitGlitchTime = 0.45f;

	// Each level's music, cut from the OST to its intro and one turn of its loop: the file's end runs on into loopStartMs.
	// The volumes level the tracks with the boss opera (BGM_Bt_Opera at 0.5).
	struct LEVEL_BGM
	{
		LEVEL level;
		const wchar_t* tag;
		uint32 loopStartMs;
		Float volume;
	};
	constexpr LEVEL_BGM LevelBgms[]
	{
		{ LEVEL::GAMEPLAY, L"BGM_CityRuins", 18000, 0.4f },
		{ LEVEL::AMUSEMENT_PARK, L"BGM_AmusementPark", 238000, 0.55f },
	};
}

Pl0000::Pl0000() : Entity{} {}
Pl0000::Pl0000(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Entity{ device, context } {}
Pl0000::Pl0000(const Pl0000& rhs)
	: Entity{ rhs }, m_LedgeReach{ rhs.m_LedgeReach }, m_LedgeMinHeight{ rhs.m_LedgeMinHeight },
	m_LedgeMaxHeight{ rhs.m_LedgeMaxHeight } {}

void Pl0000::Draw_LightWeapon()
{
	const Bool wasSheathed = m_SheathLightWeapon && m_SheathLightWeapon->Is_Active();
	if (m_LightWeapon)
	{
		m_LightWeapon->Set_Active(true);
		m_LightWeapon->DrawWP0070();
	}
	
	if (m_SheathLightWeapon)
	{
		m_SheathLightWeapon->Set_Active(false);
	}
	if (wasSheathed && m_LightHalo) m_LightHalo->Begin_Transition(false);
}
Bool Pl0000::Is_LightGround7Playing() const
{
	// The next index is the clip being played or blended into; the current one is still the sixth hit during the blend.
	const uint32 seventh = ETOI(PL0000_STATE::LIGHT_GROUND7);
	return m_MainBody && m_MainBody->Get_NextAnimationIndex() == seventh &&
		(m_MainBody->Get_CurrentAnimationIndex() != seventh || !m_MainBody->Is_AnimationFinished());
}

void Pl0000::Sheathe_LightWeapon()
{
	// The seventh hit owns the sword until it has played out and the sword has flown back. Leaving it early means a new
	// action, so the sword comes straight back to be sheathed.
	if (m_LightWeapon && m_LightWeapon->Is_Thrown())
	{
		if (Is_LightGround7Playing())
			m_LightWeapon->Recall_ThrownWeapon();
		else
		{
			m_LightWeapon->Set_Sheathing();
			return;
		}
	}
	const Bool wasDrawn = m_LightWeapon && m_LightWeapon->Is_Active();
	if (m_LightWeapon)
	{
		m_LightWeapon->Set_Active(false);
		m_LightWeapon->Set_Sheathing(); 
	}

	if (m_SheathLightWeapon)
	{
		m_SheathLightWeapon->Set_Active(true);
	}
	if (wasDrawn && m_LightHalo) m_LightHalo->Begin_Transition(true);
}

void Pl0000::Launch_FromLightGround6(Float height)
{
	if (m_Pl0000Movement)
	{
		m_Pl0000Movement->Begin_EaseOutAscent(height, 0.2f);
	}
}

void Pl0000::Draw_HeavyWeapon()
{
	const Bool wasSheathed = m_SheathHeavyWeapon && m_SheathHeavyWeapon->Is_Active();
	if (m_HeavyWeapon)
	{
		m_HeavyWeapon->Set_Active(true);
		m_HeavyWeapon->DrawWP0220();
	}

	if (m_SheathHeavyWeapon)
	{
		m_SheathHeavyWeapon->Set_Active(false);
	}
	if (wasSheathed && m_HeavyHalo) m_HeavyHalo->Begin_Transition(false);
}

void Pl0000::Sheathe_HeavyWeapon()
{
	const Bool wasDrawn = m_HeavyWeapon && m_HeavyWeapon->Is_Active();
	if (m_HeavyWeapon)
	{
		m_HeavyWeapon->Set_Active(false);
		m_HeavyWeapon->Set_Sheathing();
	}

	if (m_SheathHeavyWeapon)
	{
		m_SheathHeavyWeapon->Set_Active(true);
	}
	if (wasDrawn && m_HeavyHalo) m_HeavyHalo->Begin_Transition(true);
}

HRESULT Pl0000::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"PlayerPhysical");

	return GameObject::Initialize_Prototype();
}

HRESULT Pl0000::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg))) {
		LOG_ERROR(L"Failed to Initialize GameObject {}", m_ObjectName);
		return E_FAIL;
	}
	// Registered after GameObject::Initialize so the event goes to the level this player was placed in.
	GAME_INSTANCE->Add_Instance_Event(Get_LevelIndex(), L"Add_Owner_To_Camera", [&]()
		{
			auto camera = GAME_INSTANCE->Get_MainCamera();
			
			if (camera != nullptr)
			{
				camera->Set_Target(shared_from_this());
			}

#ifdef _DEBUG
			//Entity::DAMAGE_INFO dmgInfo{};
			//dmgInfo.attackType = ATK_TYPE::LIGHT;
			//dmgInfo.attackerPos = m_Transform->Get_Position() + Vector3{ 0.f, 0.f, -10.f };
			//dmgInfo.damage = 25.f;
			//dmgInfo.groggyWeight = 10;
			//dmgInfo.knockbackForce = 0.5f;
			//// 2. 총알 공통 속성 세팅 (정지 상태 고정)
			//Bullet::BULLET_DESC desc{};
			//desc.isPermanent = false;
			//desc.damageInfo = dmgInfo;
			//desc.resourceTag = L"candy";         // 💡 테스트용 모델이 잘 뜨는지 확인
			//desc.targetLayer = L"Player";
			//desc.speed = 0.0f;                   // 스피드 0
			//desc.maxDistance = 9999.f;
			//desc.direction = m_Transform->Get_Look();
			//
			//Float spawnHeight = m_Transform->Get_Position().y + 1.f; // 공중에 깔려면 y값을 적절히 조절하세요 (ex: 1.0f)
			//for (int x = -1; x <= 2; ++x)
			//{
			//	for (int z = -1; z <= 2; ++z)
			//	{
			//		desc.initialPosition = Vector3(static_cast<Float>(x), spawnHeight, static_cast<Float>(z));
			//		GAME_INSTANCE->Instantiate<Bullet>(L"Bullet", ETOI(LEVEL::GAMEPLAY), &desc);
			//	}
			//}
#endif

		});
	if (FAILED(Ready_PartObjects())) {
		LOG_ERROR(L"Failed to Ready PartObjects {}", m_ObjectName);
		return E_FAIL;
	}
	if (FAILED(Ready_Components())) {
		LOG_ERROR(L"Failed to Ready Components {}", m_ObjectName);
		return E_FAIL;
	}

	m_BulletLayerIndex = ETOI(GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(L"Bullet"));
	m_MonsterPhysicalLayerIndex = ETOI(GAME_INSTANCE->Get_LayerRegister()->Get_LayerByName(L"MonsterPhysical"));

	return S_OK;
}

void Pl0000::On_Destroy()
{
	if (m_IsGlobalLagActive)
	{
		m_IsGlobalLagActive = false;
		GAME_INSTANCE->Set_TimeScale(1.f);
	}
	// The Editor's Stop and a level change destroy the player but leave 2D channels playing.
	GAME_INSTANCE->StopSound(LevelBgmChannel);

	GameObject::On_Destroy();
}

void Pl0000::Start_LevelBgm()
{
	m_LevelBgmStarted = true;
	for (const LEVEL_BGM& bgm : LevelBgms)
		if (ETOI(bgm.level) == Get_LevelIndex())
			GAME_INSTANCE->PlaySoundLoopSection(bgm.tag, LevelBgmChannel, bgm.volume, bgm.loopStartMs, UINT32_MAX, true);
}

void Pl0000::Priority_Update(Float timeDelta)
{
	m_Pl0000Input->Update_Pl0000_InputState(timeDelta);
}

void Pl0000::Update(Float timeDelta)
{
	if (!m_LevelBgmStarted)
		Start_LevelBgm();

	const Float unscaledDelta = GAME_INSTANCE->Compute_UnscaledTimeDelta();
	if (m_IsGlobalLagActive)
	{
		m_GlobalLagTimer -= unscaledDelta;
		if (m_GlobalLagTimer <= 0.f)
		{
			m_IsGlobalLagActive = false;
			GAME_INSTANCE->Set_TimeScale(1.f);
		}
	}

	if (m_InvincibleTimer > 0.f)
		m_InvincibleTimer -= unscaledDelta;

	// G locks onto the nearest monster ahead of the camera; pressed again it moves to another one, or lets go when there
	// is none. Either way the lock is the player's own until it lets go.
	if (m_Pl0000Input->Is_KeyDown(UBYTE(DIKEYBOARD_G)))
	{
		const auto camera = GAME_INSTANCE->Get_MainCamera();
		const Vector3 look = camera ? camera->Get_Transform()->Get_Look() : m_Transform->Get_Look();
		const Shared<GameObject> next = m_MonsterChecker->Get_ClosestTarget(look, m_LockOnTarget.lock());
		if (next || m_IsManualLock)
			GAME_INSTANCE->PlaySoundFX(next ? L"core_target_lockon" : L"core_target_lockoff", LockOnChannel, LockOnVolume);
		m_LockOnTarget = next;
		m_IsManualLock = nullptr != next;
	}

	if (auto target = m_LockOnTarget.lock())
	{
		const auto entity = Cast<Entity>(target);
		if (target->Is_Destroy() || !target->Is_Active() || (entity && entity->Is_Dead()))
		{
			m_LockOnTarget.reset(); // 대상 파괴·사망 시 파기
		}
		else if (m_IsManualLock)
		{
			if (Vector3::Distance(m_Transform->Get_Position(), target->Get_Transform()->Get_Position()) > ManualLockRange)
				m_LockOnTarget.reset();
		}
		else
		{
			Vector3 myPos = Get_Transform()->Get_Position();
			Vector3 targetPos = target->Get_Transform()->Get_Position();
			
			Vector3 myLook = Get_Transform()->Get_Look();
			myLook.y = 0.f;
			if (myLook.Length() > 0.001f) myLook.Normalize();
			Vector3 dirToTarget = targetPos - myPos;
			dirToTarget.y = 0.f;
			if (dirToTarget.Length() > 0.001f) dirToTarget.Normalize();
			
			if (myLook.Dot(dirToTarget) <= 0.f)
			{
				m_LockOnTarget.reset();
			}
		}
	}
	if (m_LockOnTarget.expired())
		m_IsManualLock = false;

	// The pod laser is a skill on a cooldown: one press from any ground state plays the whole shot.
	if (m_PodLaserCooldown > 0.f)
		m_PodLaserCooldown -= timeDelta;
	else if (m_Pl0000Input->Is_KeyDown(UBYTE(DIKEYBOARD_E)) && m_Pl0000Movement->Is_Grounded())
	{
		const PL0000_STATE curState = m_Pl0000States->Get_CurPl0000State();
		if (curState != PL0000_STATE::JUMP && curState != PL0000_STATE::ATTACK_AIR &&
			curState != PL0000_STATE::POD_GLIDE && curState != PL0000_STATE::POD_ATTACK &&
			m_Pl0000States->Change_State(PL0000_STATE::POD_ATTACK))
			m_PodLaserCooldown = PodLaserCooldownTime;
	}

	m_Pl0000States->Update_State(timeDelta);
}

void Pl0000::Late_Update(Float timeDelta)
{
	m_Pl0000Movement->Update_Movement(timeDelta);
	m_Transform->Update_WorldMatrix();
	m_PhysicalZone->Update(m_Transform->Get_WorldMatrix());
}

void Pl0000::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT Pl0000::Render()
{
	return S_OK;
}

void Pl0000::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

void Pl0000::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void Pl0000::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	auto target = targetCollider->Get_Owner();
	if (target->Get_LayerMask().Get_Layer() != m_MonsterPhysicalLayerIndex)
		return;

	const auto monster = Cast<Monster>(target);
	if (monster && monster->Pushes_PlayerOnContact())
	{
		Vector3 pushOutDelta = PushoutDelta(ownCollider, targetCollider, 1.f);
		m_Pl0000Movement->Add_Correction(pushOutDelta);
	}
}

void Pl0000::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void Pl0000::TakeDamage(const DAMAGE_INFO& dmgInfo)
{
	const Shared<GameObject> attacker = dmgInfo.attacker.lock();
	if (TryEvade(attacker)) return;
	// The invincibility a perfect dodge leaves behind doesn't stop bullets either (Em3000's lasers, rings and homing
	// shots are on the Bullet layer): they have to be outrun or jumped.
	const Bool isBullet = attacker && attacker->Get_LayerMask().Get_Layer() == m_BulletLayerIndex;
	if (isBullet ? m_IsInvincible : Is_Invincible_Active()) return;

	Entity::TakeDamage(dmgInfo);
	const Bool heavy = dmgInfo.damage >= 30.f;
	GAME_INSTANCE->Start_ScreenGlitch(heavy ? HeavyHitGlitch : HitGlitch, heavy ? HeavyHitGlitchTime : HitGlitchTime);
	if (heavy)
		m_Pl0000States->Change_State(PL0000_STATE::HIT);
}

void Pl0000::OnAttackHit(const Shared<GameObject>& target)
{
	// The sixth light hit needs its original forward root motion through the hit window.
	if (m_Pl0000Movement && m_MainBody &&
		m_MainBody->Get_CurrentAnimationIndex() != ETOI(PL0000_STATE::LIGHT_GROUND6) &&
		m_MainBody->Get_NextAnimationIndex() != ETOI(PL0000_STATE::LIGHT_GROUND6))
	{
		m_Pl0000Movement->Reduce_RootMotion();
	}
}

Bool Pl0000::TryEvade(const Shared<GameObject>& attacker)
{
	// Bullets cannot be dodged by a dash.
	if (attacker == nullptr) return false;
	if (attacker->Get_LayerMask().Get_Layer() == m_BulletLayerIndex) return false;

	if (m_Pl0000States->Get_CurPl0000State() != PL0000_STATE::EVADE)
		return false;

	auto dashState = static_pointer_cast<State2B_Evade>(
		m_Pl0000States->Find_2BState(PL0000_STATE::EVADE));
	if (!dashState)
		return false;

	Bool isEvaded = dashState->TryEvade_FromDash();

	// A dodge locks onto the attacker unless the player has picked a target.
	if (isEvaded && attacker && !Is_ManualLockOn())
	{
		if (attacker->Get_GameObjectType() == GAMEOBJECTTYPE::PART)
		{
			m_LockOnTarget = static_pointer_cast<PartObject>(attacker)->Get_Owner();
		}
		else if (Cast<Entity>(attacker))
		{
			m_LockOnTarget = attacker;
		}
	}

	return isEvaded;
}

HRESULT Pl0000::Ready_PartObjects()
{	
	Pl0000Body::Pl0000BODY_DESC desc{};

	if (FAILED(Add_Child(Get_LevelIndex(), L"Pl0000Body", L"Pl0000Body", &desc)))
		return E_FAIL;
	if (FAILED(Add_Child(Get_LevelIndex(), L"WP0070Body", L"WP0070Body", &desc)))
		return E_FAIL;
	if (FAILED(Add_Child(Get_LevelIndex(), L"SheathWP0070Body", L"SheathWP0070Body", &desc)))
		return E_FAIL;
	if (FAILED(Add_Child(Get_LevelIndex(), L"WP0220Body", L"WP0220Body", &desc)))
		return E_FAIL;
	if (FAILED(Add_Child(Get_LevelIndex(), L"SheathWP0220Body", L"SheathWP0220Body", &desc)))
		return E_FAIL;
	if (FAILED(Add_Child(Get_LevelIndex(), L"WP3000Body", L"WP3000Body", &desc)))
		return E_FAIL;
	if (FAILED(Add_Child(Get_LevelIndex(), L"Pl0000EvadeChecker", L"Pl0000EvadeChecker", &desc)))
		return E_FAIL;
	if (FAILED(Add_Child(Get_LevelIndex(), L"Pl0000MonsterChecker", L"Pl0000MonsterChecker", &desc)))
		return E_FAIL;

	m_MainBody = static_pointer_cast<Pl0000Body>(Find_Child(L"Pl0000Body"));
	m_MonsterChecker = static_pointer_cast<Pl0000MonsterChecker>(Find_Child(L"Pl0000MonsterChecker"));

	m_LightWeapon = static_pointer_cast<WP0070Body>(Find_Child(L"WP0070Body"));
	m_SheathLightWeapon = static_pointer_cast<SheathWP0070Body>(Find_Child(L"SheathWP0070Body"));
	m_HeavyWeapon = static_pointer_cast<WP0220Body>(Find_Child(L"WP0220Body"));
	m_SheathHeavyWeapon = static_pointer_cast<SheathWP0220Body>(Find_Child(L"SheathWP0220Body"));

	// 헤일로·검기·HP바도 플레이어가 만드는 자식이라 씬에는 StableChildKey로만 남는다.
	WeaponHalo::WEAPON_HALO_DESC heavyHalo{};
	heavyHalo.anchor = m_SheathHeavyWeapon;
	heavyHalo.handAnchor = m_HeavyWeapon;
	heavyHalo.bladeStart = -0.6f;
	heavyHalo.bladeEnd = -1.82f;
	// The halo rides the blade frame (bone0, blade along -Z), so it only slides along the blade.
	// Halo_core093.model is core.eff/093 in its own units (circuit radius 0.175); the light sword takes 092, half of it.
	// Both are drawn 1.5 times that size on request (2026-10-05), so the heavy circuit reaches 0.26 from the blade axis.
	// Positions measured on the weapon meshes, whose cross-sections are centred on the blade axis: the heavy ring
	// circles the chained guard (wp0220 0.40~0.60 from bone0, half-width 0.156), the light ring the grip just behind
	// the tsuba (wp0070 +0.15, radius 0.098).
	heavyHalo.localMatrix = Matrix::CreateScale(1.5f) *
		Matrix::CreateTranslation(Vector3{ 0.f, 0.f, -0.5f });
	if (FAILED(Add_Child(Get_LevelIndex(), L"WeaponHalo", L"WeaponHaloHeavy", &heavyHalo)))
		return E_FAIL;
	m_HeavyHalo = static_pointer_cast<WeaponHalo>(Find_Child(L"WeaponHaloHeavy"));
	WeaponHalo::WEAPON_HALO_DESC lightHalo{};
	lightHalo.anchor = m_SheathLightWeapon;
	lightHalo.handAnchor = m_LightWeapon;
	lightHalo.bladeStart = -0.2f;
	lightHalo.bladeEnd = -1.155f;
	lightHalo.localMatrix = Matrix::CreateScale(0.75f) *
		Matrix::CreateTranslation(Vector3{ 0.f, 0.f, -0.05f });
	if (FAILED(Add_Child(Get_LevelIndex(), L"WeaponHalo", L"WeaponHaloLight", &lightHalo)))
		return E_FAIL;
	m_LightHalo = static_pointer_cast<WeaponHalo>(Find_Child(L"WeaponHaloLight"));

	Pl0000HpBar::PL0000_HP_BAR_DESC hpBarDesc{};
	hpBarDesc.target = static_pointer_cast<Entity>(shared_from_this());
	hpBarDesc.anchor = UI_ANCHOR::TOP_LEFT;
	hpBarDesc.x = 300.f;
	hpBarDesc.y = 80.f;
	hpBarDesc.sizeX = 400.f;
	hpBarDesc.sizeY = 15.f;
	if (FAILED(Add_Child(Get_LevelIndex(), L"Pl0000HpBar", L"Pl0000HpBar", &hpBarDesc)))
		return E_FAIL;

	// Pod program R010: the beam and its impacts, fired by State2B_PodAttack.
	GameObject::GAMEOBJECT_DESC laserEffectDesc{};
	if (FAILED(Add_Child(Get_LevelIndex(), L"LaserEffect", L"LaserEffect", &laserEffectDesc)))
		return E_FAIL;
	PodLaser::POD_LASER_DESC podLaserDesc{};
	podLaserDesc.impact = static_pointer_cast<LaserEffect>(Find_Child(L"LaserEffect"));
	if (FAILED(Add_Child(Get_LevelIndex(), L"PodLaser", L"PodLaser", &podLaserDesc)))
		return E_FAIL;
	for (size_t i = 0; i < m_SwordWaves.size(); ++i)
	{
		GameObject::GAMEOBJECT_DESC waveDesc{};
		const wstring key = L"SwordWave" + to_wstring(i);
		if (FAILED(Add_Child(Get_LevelIndex(), L"SwordWave", key, &waveDesc)))
			return E_FAIL;
		m_SwordWaves[i] = static_pointer_cast<SwordWave>(Find_Child(key));
	}
	ParticleEffect::PARTICLE_EFFECT_DESC fireFlashDesc{};
	if (FAILED(Add_Child(Get_LevelIndex(), L"FireFlashEffect", L"FireFlashEffect", &fireFlashDesc)))
		return E_FAIL;

	auto wp3000 = static_pointer_cast<WP3000Body>(Find_Child(L"WP3000Body"));
	wp3000->Set_Pl0000Body(m_MainBody);
	wp3000->Set_FireFlash(static_pointer_cast<FireFlashEffect>(Find_Child(L"FireFlashEffect")));

	return S_OK;
}

HRESULT Pl0000::Ready_Components()
{
	m_Pl0000Input = Add_Component<Pl0000Input>(ETOI(LEVEL::GAMEPLAY));
	if (nullptr == m_Pl0000Input)
		return E_FAIL;

	Pl0000Movement::PL0000_MOVEMENT_DESC movementDesc{};
	movementDesc.velocity = Vector3{0.f, 0.f, 0.f};
	movementDesc.moveSpeed = 0.f;
	movementDesc.targetDirection = Vector3::Zero;
	movementDesc.turnSpeed = 7.5f;
	m_Pl0000Movement = Add_Component<Pl0000Movement>(ETOI(LEVEL::GAMEPLAY), &movementDesc);
	if (nullptr == m_Pl0000Movement)
		return E_FAIL;

	// StateMachine은 마지막에 처리
	if ((m_Pl0000States = Add_Component<Pl0000StateMachine>(ETOI(LEVEL::GAMEPLAY))))
	{
		auto pl0000 = static_pointer_cast<Pl0000>(shared_from_this());
		// The amusement park (Level 4) plays A2 (Pl0000Body): only her right-click attacks differ from 2B's.
		const Bool isA2 = Get_LevelIndex() == ETOI(LEVEL::AMUSEMENT_PARK);
		const wstring attackGroundTag = Helper::To_wString(magic_enum::enum_name(PL0000_STATE::ATTACK_GROUND));
		const wstring attackAirTag = Helper::To_wString(magic_enum::enum_name(PL0000_STATE::ATTACK_AIR));

		if (FAILED(m_Pl0000States->Add_State(State2B_Idle::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::IDLE)), pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(State2B_Run::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::RUN)), pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(State2B_Sprint::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::SPRINT)), pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(State2B_Jump::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::JUMP)), pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(State2B_Evade::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::EVADE)), pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(isA2
			? static_pointer_cast<State2B_AttackGround>(StateA2_AttackGround::Create(attackGroundTag, pl0000))
			: State2B_AttackGround::Create(attackGroundTag, pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(isA2
			? static_pointer_cast<State2B_AttackAir>(StateA2_AttackAir::Create(attackAirTag, pl0000))
			: State2B_AttackAir::Create(attackAirTag, pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(State2B_Hit::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::HIT)), pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(State2B_PodGlide::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::POD_GLIDE)), pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(State2B_PodAttack::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::POD_ATTACK)), pl0000))))
			return E_FAIL;
		if (FAILED(m_Pl0000States->Add_State(State2B_Ledge::Create(
			Helper::To_wString(magic_enum::enum_name(PL0000_STATE::LEDGE)), pl0000))))
			return E_FAIL;

		m_Pl0000States->Change_State(PL0000_STATE::IDLE);
	}
	else
		return E_FAIL;

	m_Pl0000Movement->Begin();
	m_Pl0000Input->Begin();

	SphereCollider::SPHERE_COLLIDER_DESC physicalZoneDesc{};
	physicalZoneDesc.offset = Vector3::UnitY;
	physicalZoneDesc.radius = 0.75f;
	m_PhysicalZone = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &physicalZoneDesc);
	if (nullptr == m_PhysicalZone)
		return E_FAIL;

	return S_OK;
}

void Pl0000::Trigger_GlobalLag(Float timeScale, Float duration)
{
	if (m_IsGlobalLagActive && m_GlobalLagTimer >= duration)
		return;

	m_GlobalLagTimer = duration;
	m_IsGlobalLagActive = true;
	GAME_INSTANCE->Set_TimeScale(timeScale);
}

void Pl0000::Swing_SwordWave(const Shared<Pl0000Parts>& weapon)
{
	// The wave measures the swing at the blade tip on bone0 (engine Z = -wmb Z): wp0070 ends at -1.155, wp0220 at -1.82.
	const Bool heavy = weapon == m_HeavyWeapon;
	// Light swings take their wave's roll (degrees) from the swing's wpf000 record, the z rotation of the first FWK
	// with a rotation: pl0000_0100..0105 -> wpf000 010..015, air 0130..0132 -> 040..042, run 0180 -> 051.
	using LIGHT = WP0070Body::WP0070_STATE;
	static const unordered_map<uint32, Float> LightWaveRolls{
		{ ETOI(LIGHT::LIGHT_GROUND1), -50.f }, { ETOI(LIGHT::LIGHT_GROUND2), 225.f }, { ETOI(LIGHT::LIGHT_GROUND3), 0.f },
		{ ETOI(LIGHT::LIGHT_GROUND4), 0.f }, { ETOI(LIGHT::LIGHT_GROUND5), 0.f }, { ETOI(LIGHT::LIGHT_GROUND6), 0.f },
		{ ETOI(LIGHT::LIGHT_AIR1), -55.f }, { ETOI(LIGHT::LIGHT_AIR2), 225.f }, { ETOI(LIGHT::LIGHT_AIR3), 10.f },
		{ ETOI(LIGHT::LIGHT_GROUND_RUN), -60.f },
	};
	optional<Float> roll{};
	if (!heavy)
		if (const auto found = LightWaveRolls.find(weapon->Get_CurrentAnimationIndex()); found != LightWaveRolls.end())
			roll = found->second;
	for (const auto& wave : m_SwordWaves)
		if (wave && wave->Is_Idle())
		{
			wave->Swing(weapon, heavy ? Vector3{ -0.05f, 0.f, -1.82f } : Vector3{ 0.f, 0.f, -1.155f },
				heavy ? ATK_TYPE::HEAVY : ATK_TYPE::LIGHT, roll);
			return;
		}
}

Shared<Pl0000> Pl0000::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	Shared<Pl0000> pInstance = make_shared<Pl0000>(device, context);

	if (FAILED(pInstance->Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Created : 2B");
		MSG_BOX("Failed to Created : 2B");
	}
	return pInstance;
}

Shared<GameObject> Pl0000::Clone(void* arg)
{
	Shared<Pl0000> pInstance = make_shared<Pl0000>(*this);

	if (FAILED(pInstance->Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Cloned : 2B");
		MSG_BOX("Failed to Cloned : 2B");
	}
	return pInstance;
}

}
