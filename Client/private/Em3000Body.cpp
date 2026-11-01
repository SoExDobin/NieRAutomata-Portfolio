#include "pch.h"
#include "Em3000Body.h"

#include <Game.h>
#include <SpdLogger.h>

#include <Shader.h>
#include <SoundEmitter.h>
#include <SphereCollider.h>

#include "Entity.h"
#include "Bullet.h"
#include "ClientSettingManager.h"
#include "Em3000.h"
#include "Em3000Homing.h"
#include "EmRing.h"
#include "ExplodeEffect.h"
#include "Monster.h"
#include "MonsterAOE.h"
#include "Random_Helper.h"

Em3000Body::Em3000Body() : PartObject{} {}
Em3000Body::Em3000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{device, context} {}
Em3000Body::Em3000Body(const Em3000Body& rhs)
	: PartObject{ rhs } {}

const TRANSFORM_FRAME& Em3000Body::Get_ModelTransform() const
{
	return m_Model->Get_RootTransformVelocity(m_RootBoneIndex);
}

void Em3000Body::Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop)
{
	m_Model->Set_Animation(animIndex, blendDuration);
	m_Model->Set_AnimLoop(isLoop);
}

uint32 Em3000Body::Get_CurrentAnimationIndex() const
{
	return m_Model->Get_AnimationIndex();
}

uint32 Em3000Body::Get_NextAnimationIndex() const
{
	return m_Model->Get_NextAnimationIndex();
}

Float Em3000Body::Get_AnimationProgress() const
{
	return m_Model->Get_AnimationProgress();
}

Bool Em3000Body::Is_AnimationFinished() const
{
	return m_Model->Is_AnimationFinished();
}

HRESULT Em3000Body::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Monster");
	m_TagMask.Set_Tag({ L"Monster" });

	return PartObject::Initialize_Prototype();
}

HRESULT Em3000Body::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : Em3000Body");
		return E_FAIL;
	}

	if (FAILED(Ready_AnimationNotify()))
	{
		LOG_ERROR(L"Failed To Ready_AnimationNotify : Em3000Body");
		return E_FAIL;
	}

	m_RootBoneIndex = m_Model->Get_BoneIndexByName("em3000");

	if (m_RootBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find Em3000 Root Bone");
		return E_FAIL;
	}

	m_Model->Set_LocalRootNode(m_RootBoneIndex);
	// The singer's chest, where the homing lasers leave from: bone5 (5.7 m up at rest), where both clavicles branch off the
	// spine. bone6 and bone7 above it are the neck and bone16 the head.
	m_ChestBoneIndex = m_Model->Get_BoneIndexByName("bone5");
	m_MuzzleBoneIndex = m_Model->Get_BoneIndexByName("bone773");
	if (m_MuzzleBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find Em3000 Gun Port Bone");
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Em3000Body::Begin()
{
	if (const Shared<GameObject> owner = Get_Owner())
		m_BodyAttack = static_pointer_cast<MonsterAOE>(owner->Find_Child(L"Em3000CenterAoe"));
	// The skirt's hit volume starts off and only switches on for the front stomp and the blade mixer. Left on, it hurt a
	// player touching the idle boss and set off Pl0000EvadeChecker's perfect dodge in front of it.
	Set_BodyAttack(false);
	return S_OK;
}

void Em3000Body::Begin_Barrage(Bullet::FLIGHT flight)
{
	m_IsBarrage = true;
	m_BarrageFlight = flight;
	m_BarrageTimer = 0.f;
	m_BarrageVolley = 0;
}

void Em3000Body::End_Barrage()
{
	m_IsBarrage = false;
}

void Em3000Body::Fire_Howitzer(Float speed)
{
	const Shared<GameObject> owner = Get_Owner();
	if (!owner) return;

	m_SoundEmitter->Play(L"em3000_missile_shot");
	m_SoundEmitter->Play(L"em3000_0190_event");
	for (uint32 i = 0; i < NumMuzzles; ++i)
	{
		Bullet::BULLET_DESC desc{};
		Get_Muzzle(i, desc.initialPosition, desc.direction);
		desc.damageInfo = Make_BulletDamage(30.f);
		// The shells come down nearly together, so only a few sound their burst.
		if (i % HowitzerBurstSoundEvery == 0)
			desc.burstSound = L"em3000_shot_bom_explo";
		desc.resourceTag = L"candy";
		desc.targetLayer = L"Player";
		desc.isPermanent = true;
		desc.flight = Bullet::FLIGHT::ARC;
		desc.gravity = 30.f;
		desc.groundY = owner->Get_Transform()->Get_Position().y;
		desc.speed = speed;
		desc.maxDistance = 60.f;
		Bullet::Fire_Spread(desc, PortShots, PortSpread, 0.f, Get_LevelIndex());
	}
}

void Em3000Body::Begin_Homing()
{
	m_HomingLeft = HomingCount;
	m_HomingTimer = 0.f;
}

void Em3000Body::Fire_Homing(Float timeDelta)
{
	if (m_HomingLeft == 0) return;
	m_HomingTimer -= timeDelta;
	if (m_HomingTimer > 0.f) return;
	m_HomingTimer += HomingInterval;

	const Shared<Monster> owner = Cast<Monster>(Get_Owner());
	if (!owner) return;
	const uint32 index = HomingCount - m_HomingLeft--;
	const Vector3 position = owner->Get_Transform()->Get_Position();
	const Vector3 chest = m_ChestBoneIndex >= 0
		? (m_Model->Get_BoneMatrix(m_ChestBoneIndex) * m_CombinedWorldMatrix).Translation()
		: position + Vector3{ 0.f, 5.f, 0.f };
	const Vector3 look = owner->Get_Transform()->Get_Look();
	// The shots leave in turn across a tight fan over the front (the boss faces the player), all tipped steeply up, so
	// they come in one after another from one side instead of closing in from every direction.
	const Float yaw = atan2f(look.x, look.z) +
		XMConvertToRadians(-HomingSpread * 0.5f + HomingSpread * index / (HomingCount - 1));
	Em3000Homing::EM3000_HOMING_DESC desc{};
	desc.target = owner->Get_Target();
	desc.position = chest;
	desc.direction = Vector3{ sinf(yaw) * 0.6f, 1.f, cosf(yaw) * 0.6f };
	desc.groundY = position.y;
	GAME_INSTANCE->Instantiate<Em3000Homing>(L"Em3000Homing", Get_LevelIndex(), &desc);
	m_SoundEmitter->Play(L"em3000_homing_laser_shot");
}

void Em3000Body::Burst_Rings() const
{
	const Shared<Monster> owner = Cast<Monster>(Get_Owner());
	if (!owner) return;

	// Reference video 1:40.5-1:47.5: a ring flashes out of the head about every 0.7 s, seven in all. Each opens out wider
	// than the last while it falls, so they land and burst one after another, from just outside the skirt (2.8 m at
	// idle) out to the inside wall of the amusement park's theatre dome (G11716, about 25 m); the wider ones fly longer.
	EmRing::EM_RING_DESC desc{};
	desc.mode = EmRing::MODE::BURST;
	desc.target = owner->Get_Target();
	desc.start = owner->Get_Transform()->Get_Position();
	desc.count = 7;
	desc.interval = 0.7f;
	desc.startRadius = 1.5f;
	desc.firstRadius = 4.f;
	desc.lastRadius = 25.f;		// the dome's inner wall
	desc.firstFall = 2.4f;		// floating up, then falling
	desc.lastFall = 3.2f;
	desc.tint = Vector3{ 1.1f, 0.16f, 0.3f };
	desc.damage = 25.f;
	desc.burstSound = L"em3000_shot_bom_explo";
	desc.glitch = 0.5f;
	GAME_INSTANCE->Instantiate<EmRing>(L"EmRing", Get_LevelIndex(), &desc);
}

void Em3000Body::Spread_StompRing() const
{
	const Shared<Monster> owner = Cast<Monster>(Get_Owner());
	if (!owner) return;

	EmRing::EM_RING_DESC desc{};
	desc.mode = EmRing::MODE::GROUND;
	desc.target = owner->Get_Target();
	desc.start = owner->Get_Transform()->Get_Position();
	desc.spreadSpeed = 9.f;
	desc.endRadius = 25.f;
	desc.haze = 1.f;
	desc.glitch = 0.7f;
	desc.tint = Vector3{ 1.f, 0.5f, 0.25f };
	desc.damage = 25.f;
	GAME_INSTANCE->Instantiate<EmRing>(L"EmRing", Get_LevelIndex(), &desc);
}

void Em3000Body::Set_BodyAttack(Bool active, Float radius)
{
	const Shared<MonsterAOE> attack = m_BodyAttack.lock();
	if (!attack) return;
	if (const Shared<SphereCollider> sphere = attack->Get_Component<SphereCollider>())
	{
		if (m_BodyAttackRadius > 0.f)
		{
			sphere->Set_Radius(m_BodyAttackRadius);
			m_BodyAttackRadius = 0.f;
		}
		if (active && radius > 0.f)
		{
			m_BodyAttackRadius = sphere->Get_Radius();
			sphere->Set_Radius(radius);
		}
	}
	if (active)
		attack->Active_Attack();
	else
		attack->DeActive_Attack();
}

void Em3000Body::Play_LoopSound(const wstring& soundTag)
{
	Stop_LoopSound();
	m_LoopSound = GAME_INSTANCE->PlaySound3D(soundTag, m_SoundEmitter->Get_Volume(), m_CombinedWorldMatrix.Translation(),
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), true);
	m_LoopLaps = 0;
}

void Em3000Body::Stop_LoopSound()
{
	if (m_LoopSound == 0) return;
	GAME_INSTANCE->StopLoopSound3D(m_LoopSound);
	m_LoopSound = 0;
}

void Em3000Body::End_Attacks()
{
	End_Barrage();
	m_HomingLeft = 0;
	Set_BodyAttack(false);
	Stop_LoopSound();
}

void Em3000Body::Burst(Float size, Bool atChest) const
{
	const int32 bone = atChest ? m_ChestBoneIndex : Helper::Random_Int(0, static_cast<int32>(m_Model->Get_Num_Bones()) - 1);
	ExplodeEffect::EXPLODE_EFFECT_DESC blast{};
	blast.position = (m_Model->Get_BoneMatrix(bone) * m_CombinedWorldMatrix).Translation();
	blast.size = size;
	GAME_INSTANCE->Instantiate<ExplodeEffect>(L"ExplodeEffect", Get_LevelIndex(), &blast);
	GAME_INSTANCE->PlaySound3D(L"em_common_death_bom_m", m_SoundEmitter->Get_Volume(), blast.position,
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), false);
}

void Em3000Body::Play_Voice(const wstring& soundTag) const
{
	GAME_INSTANCE->PlaySound3D(soundTag, m_SoundEmitter->Get_Volume() * VoiceVolume, m_CombinedWorldMatrix.Translation(),
		VoiceNear, m_SoundEmitter->Get_FarDistance(), false);
}

void Em3000Body::Play_HurtVoice()
{
	if (m_HurtVoiceCooldown > 0.f) return;
	m_HurtVoiceCooldown = HurtVoiceInterval;
	Play_Voice(L"em3000_vo_l_dmg");
}

Vector3 Em3000Body::Get_ChestPosition() const
{
	return (m_Model->Get_BoneMatrix(m_ChestBoneIndex) * m_CombinedWorldMatrix).Translation();
}

void Em3000Body::Begin_Shatter(const Vector3& center)
{
	m_Shatter.Begin(m_Model, center);
}

void Em3000Body::Off_Colliders() const
{
	m_HitBox->Set_Active(false);
}

void Em3000Body::Fire_Barrage(Float timeDelta)
{
	if (!m_IsBarrage) return;
	m_BarrageTimer += timeDelta;
	if (m_BarrageTimer < BarrageInterval) return;
	m_BarrageTimer -= BarrageInterval;

	const Bool permanent = (++m_BarrageVolley % 4) == 0;
	for (uint32 i = 0; i < NumMuzzles; ++i)
	{
		Bullet::BULLET_DESC desc{};
		Get_Muzzle(i, desc.initialPosition, desc.direction);
		desc.damageInfo = Make_BulletDamage(10.f);
		desc.resourceTag = L"candy";
		desc.targetLayer = L"Player";
		desc.isPermanent = permanent;
		desc.flight = m_BarrageFlight;
		desc.speed = BarrageSpeed;
		desc.maxDistance = BarrageRange;
		if (m_BarrageFlight != Bullet::FLIGHT::STRAIGHT)
		{
			Bullet::Fire_Spread(desc, PortShots, PortSpread, 0.f, Get_LevelIndex());
			continue;
		}
		const Vector3 cross = desc.initialPosition + desc.direction * CrossDistance;
		Vector3 side = Vector3::UnitY.Cross(desc.direction);
		side.Normalize();
		for (const Float sign : { -1.f, 1.f })
		{
			Bullet::BULLET_DESC shot = desc;
			shot.initialPosition = desc.initialPosition + side * (sign * CrossHalfWidth);
			shot.direction = cross - shot.initialPosition;
			shot.direction.Normalize();
			GAME_INSTANCE->Instantiate<Bullet>(L"Bullet", Get_LevelIndex(), &shot);
		}
	}
}

void Em3000Body::Get_Muzzle(uint32 index, Vector3& position, Vector3& direction) const
{
	const Vector3& offset = MuzzleOffsets[index];
	const Matrix toWorld = m_Model->Get_BoneMatrix(m_MuzzleBoneIndex) * m_CombinedWorldMatrix;
	position = Vector3::Transform(offset, toWorld);
	direction = Vector3::TransformNormal(Vector3{ offset.x, 0.f, offset.z }, toWorld);
	direction.y = 0.f;
	direction.Normalize();
}

Entity::DAMAGE_INFO Em3000Body::Make_BulletDamage(Float damage) const
{
	Entity::DAMAGE_INFO dmgInfo{};
	dmgInfo.attacker = Get_Owner();
	dmgInfo.attackType = ATK_TYPE::LIGHT;
	dmgInfo.damage = damage;
	return dmgInfo;
}

void Em3000Body::On_Destroy()
{
	Stop_LoopSound();
	PartObject::On_Destroy();
}

void Em3000Body::Priority_Update(Float timeDelta)
{
	
}

void Em3000Body::Update(Float timeDelta)
{
	m_HurtVoiceCooldown -= timeDelta;
	// The pieces keep the pose the boss died in.
	if (m_Shatter.Is_Running())
	{
		m_Shatter.Update(timeDelta);
		return;
	}
	m_Model->Update_ModelAnimation(timeDelta * m_AnimationRate);
}

void Em3000Body::Late_Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	m_HitBox->Update(m_CombinedWorldMatrix);
	Fire_Barrage(timeDelta);
	Fire_Homing(timeDelta);
}

void Em3000Body::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT Em3000Body::Render()
{
	// Flying pieces cast no shadow; the frozen pose would leave a standing shadow behind.
	const Bool shattering = m_Shatter.Is_Running();
	if (shattering && GAME_INSTANCE->Is_ShadowPass())
		return S_OK;
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;
	if (shattering && FAILED(m_Shatter.Bind(m_Shader)))
		return E_FAIL;

	size_t numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i))) return E_FAIL;

		if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
			return E_FAIL;
		if (shattering)
		{
			const HRESULT hr = m_Shatter.Begin_Mesh(m_Shader, i);
			if (FAILED(hr)) return E_FAIL;
			if (S_FALSE == hr) continue;
		}

		if (FAILED(m_Model->Render(i))) return E_FAIL;
	}

	return S_OK;
}

void Em3000Body::Submit_RenderGroup()
{
	// Also runs in edit mode, where Late_Update does not: keep the mesh on the owner's transform.
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	// The boss object stays; once its pieces have dissolved there is nothing left to draw.
	if (m_Shatter.Is_Gone())
		return;
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

void Em3000Body::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void Em3000Body::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void Em3000Body::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

HRESULT Em3000Body::Bind_ShaderResources()
{
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_CombinedWorldMatrix)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	return S_OK;
}

HRESULT Em3000Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXANIMMESH::Tag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"em3000" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;
	if (m_Model->Get_AnimationPreset().Is_Empty())
	{
		AnimationPresetSnapshot preset;
		if (FAILED(ClientSettingManager::GetInstance()->Load_AnimationPreset(L"em3000", preset)) ||
			FAILED(m_Model->Apply_AnimationPreset(preset)))
			return E_FAIL;
	}

	// The skirt: its bell has a 2.75 m radius at the hem and narrows to 1.2 m at the 4.4 m waist. The upper body has its
	// own sphere (Em3000UpperHitBox). In phase 1 the skirt turns Pod shots aside (Em3000::Deflects).
	SphereCollider::SPHERE_COLLIDER_DESC sphereDesc{};
	sphereDesc.radius = 2.7f;
	sphereDesc.offset = Vector3{ 0.f, 1.6f, 0.f };
	SoundEmitter::SOUND_EMITTER_DESC emitterDesc{};
	emitterDesc.volume = 0.3f;
	emitterDesc.nearDistance = 10.f;
	emitterDesc.farDistance = 80.f;
	m_SoundEmitter = Add_Component<SoundEmitter>(ETOI(LEVEL::STATIC), &emitterDesc);
	if (nullptr == m_SoundEmitter)
		return E_FAIL;

	m_HitBox = Add_Component<SphereCollider>(ETOI(LEVEL::STATIC), &sphereDesc);
	if (nullptr == m_HitBox)
		return E_FAIL;

	return S_OK;
}

HRESULT Em3000Body::Ready_AnimationNotify()
{
	using anim = Em3000::EM3000_ANIM;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	// 원작 SeTrack 사운드 이벤트는 몸의 SoundEmitter로 낸다(카메라와의 거리만큼 줄어든다).
	auto playSound = [this](const tChar* tag) { m_SoundEmitter->Play(tag); };
	// Her voice and howl louder and heard farther (Play_Voice).
	auto playVoice = [this](const tChar* tag) { Play_Voice(tag); };

	// Frames are 60 per second. Front stomp lands at 65; the howitzer volleys follow the 05c1 skirt pulses.
	m_Model->Add_AnimNotify(ETOI(anim::FRONT_STOMP), {
		notify{ L"em3000_skirt_open", 0.f, [playSound]() { playSound(L"em3000_skirt_open"); } },
		notify{ L"em3000_vo_m_atk", 0.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
		notify{ L"em3000_foot_stomp_start", 8.f, [playSound]() { playSound(L"em3000_foot_stomp_start"); } },
		notify{ L"em3000_vo_m_atk", 48.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
		notify{ L"em3000_foot_stomp_impact", 58.f, [playSound]() { playSound(L"em3000_foot_stomp_impact"); } },
		notify{ L"em3000_foot_stomp_end", 105.f, [playSound]() { playSound(L"em3000_foot_stomp_end"); } },
		notify{ L"em3000_skirt_close", 174.f, [playSound]() { playSound(L"em3000_skirt_close"); } },
		notify{ L"Stomp_Impact", 65.f, 75.f, [this]() { Set_BodyAttack(true); }, [this]() { Set_BodyAttack(false); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_LOOP_3), {
		notify{ L"em3000_vo_laugh", 210.f, [playVoice]() { playVoice(L"em3000_vo_laugh"); } },
		notify{ L"em3000_vo_h_charge_atk", 20.f, [playVoice]() { playVoice(L"em3000_vo_h_charge_atk"); } },
		notify{ L"Howitzer1", 20.f, [this]() { Fire_Howitzer(8.f); } },
		notify{ L"Howitzer2", 70.f, [this]() { Fire_Howitzer(12.f); } },
		notify{ L"Howitzer3", 120.f, [this]() { Fire_Howitzer(8.f); } },
		notify{ L"Howitzer4", 170.f, [this]() { Fire_Howitzer(3.f); } },
		notify{ L"Howitzer5", 220.f, [this]() { Fire_Howitzer(8.f); } },
		notify{ L"Howitzer6", 270.f, [this]() { Fire_Howitzer(3.f); } },
		notify{ L"Howitzer7", 320.f, [this]() { Fire_Howitzer(12.f); } },
	});
	// The first ring flashes out of the head at 112 (reference video 1:40.5). 0560 seq puts em3000_EMP_around_shot (4 s)
	// there too, but on its own it sounded wrong at the throw: each ring sounds as it bursts instead (EmRing burstSound).
	m_Model->Add_AnimNotify(ETOI(anim::RING_THROW), {
		notify{ L"em3000_vo_h_charge", 0.f, [playVoice]() { playVoice(L"em3000_vo_h_charge"); } },
		notify{ L"em3000_EMP_around_shot_charge", 55.f, [playSound]() { playSound(L"em3000_EMP_around_shot_charge"); } },
		notify{ L"em3000_vo_sing", 112.f, [playVoice]() { playVoice(L"em3000_vo_sing"); } },
		notify{ L"Ring_Burst", 112.f, [this]() { Burst_Rings(); } },
	});
	/* 1500 seq AttackTrack: the landing hitboxes at 1.42-1.58 s, four 6 x 1 x 6 half-extent boxes on the root turned 0,
	   23, 45 and 69 degrees (about a 6.5 m disc) from 1.5 s, and a 3 x 1 x 3 one on the drum (bone773) from 1.42 s. The
	   skirt's sphere grown to the disc stands in for them over the whole span. */
	m_Model->Add_AnimNotify(ETOI(anim::JUMP_STOMP), {
		notify{ L"em3000_jump_stomp_start", 0.f, [playSound]() { playSound(L"em3000_jump_stomp_start"); } },
		notify{ L"em3000_vo_h_charge_atk", 31.f, [playVoice]() { playVoice(L"em3000_vo_h_charge_atk"); } },
		notify{ L"em3000_jump_stomp_fall_wsh", 65.f, [playSound]() { playSound(L"em3000_jump_stomp_fall_wsh"); } },
		notify{ L"em3000_jump_stomp_impact", 86.f, [playSound]() { playSound(L"em3000_jump_stomp_impact"); } },
		notify{ L"em3000_step", 134.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 134.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_jump_stomp_end", 148.f, [playSound]() { playSound(L"em3000_jump_stomp_end"); } },
		notify{ L"em3000_step", 150.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 150.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"Stomp_Ring", 88.f, [this]() { Spread_StompRing(); } },
		notify{ L"Stomp_Land", 85.f, 95.f, [this]() { Set_BodyAttack(true, StompLandRadius); }, [this]() { Set_BodyAttack(false); } },
	});

	m_Model->Add_AnimNotify(ETOI(anim::BEFORE_GROGGY), {
		notify{ L"em_common_death_bom_m", 0.f, [playSound]() { playSound(L"em_common_death_bom_m"); } },
		notify{ L"em3000_dmg_fire_loop", 1.f, [playSound]() { playSound(L"em3000_dmg_fire_loop"); } },
		notify{ L"em3000_vo_death", 25.f, [playVoice]() { playVoice(L"em3000_vo_death"); } },
		notify{ L"em3000_vo_cry", 200.f, [playVoice]() { playVoice(L"em3000_vo_cry"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::GROGGY_IN), {
		notify{ L"em3000_dizzy_start", 1.f, [playSound]() { playSound(L"em3000_dizzy_start"); } },
		notify{ L"em3000_vo_quiet_death", 1.f, [playVoice]() { playVoice(L"em3000_vo_quiet_death"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::GROGGY_END), {
		notify{ L"em3000_dizzy_end", 0.f, [playSound]() { playSound(L"em3000_dizzy_end"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::HAND_WAVE), {
		notify{ L"em3000_skirt_open_short", 13.f, [playSound]() { playSound(L"em3000_skirt_open_short"); } },
		notify{ L"em3000_vo_m_atk", 65.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
		notify{ L"em3000_skirt_close_short", 186.f, [playSound]() { playSound(L"em3000_skirt_close_short"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::MIXER_START), {
		notify{ L"em3000_skirt_open_short", 3.f, [playSound]() { playSound(L"em3000_skirt_open_short"); } },
		notify{ L"em3000_blade_atk_start", 37.f, [playSound]() { playSound(L"em3000_blade_atk_start"); } },
		notify{ L"em3000_knife_atk_start", 37.f, [playSound]() { playSound(L"em3000_knife_atk_start"); } },
		notify{ L"em3000_vo_m_atk", 37.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
	});
	// The whirr (em3000_knife_atk_loop) is held over the whole spin by StateEm3000_Melee; she laughs on the first lap and
	// every MixerLaughEvery laps after it.
	m_Model->Add_AnimNotify(ETOI(anim::MIXING), {
		notify{ L"em3000_vo_laugh", 74.f, [this, playVoice]() { if (0 == m_LoopLaps++ % MixerLaughEvery) playVoice(L"em3000_vo_laugh"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::MIXER_END), {
		notify{ L"em3000_knife_atk_end", 26.f, [playSound]() { playSound(L"em3000_knife_atk_end"); } },
		notify{ L"em3000_skirt_close_short", 62.f, [playSound]() { playSound(L"em3000_skirt_close_short"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::RING_CHARGE), {
		notify{ L"em3000_vo_sing", 0.f, [playVoice]() { playVoice(L"em3000_vo_sing"); } },
		notify{ L"em3000_soprano_shot_charge", 1.f, [playSound]() { playSound(L"em3000_soprano_shot_charge"); } },
		notify{ L"em3000_vo_sing", 600.f, [playVoice]() { playVoice(L"em3000_vo_sing"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_START_1), {
		notify{ L"em3000_skirt_open_short", 3.f, [playSound]() { playSound(L"em3000_skirt_open_short"); } },
		notify{ L"em3000_vo_m_atk", 3.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_LOOP_1), {
		notify{ L"em3000_vo_laugh", 100.f, [playVoice]() { playVoice(L"em3000_vo_laugh"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_END_1), {
		notify{ L"em3000_skirt_close_round", 42.f, [playSound]() { playSound(L"em3000_skirt_close_round"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_START_2), {
		notify{ L"em3000_skirt_open_short", 3.f, [playSound]() { playSound(L"em3000_skirt_open_short"); } },
		notify{ L"em3000_vo_m_atk", 3.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
		notify{ L"em3000_vo_h_charge", 60.f, [playVoice]() { playVoice(L"em3000_vo_h_charge"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_LOOP_2), {
		notify{ L"em3000_vo_laugh", 139.f, [playVoice]() { playVoice(L"em3000_vo_laugh"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_END_2), {
		notify{ L"em3000_skirt_close_round", 42.f, [playSound]() { playSound(L"em3000_skirt_close_round"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_START_3), {
		notify{ L"em3000_skirt_open_short", 3.f, [playSound]() { playSound(L"em3000_skirt_open_short"); } },
		notify{ L"em3000_vo_m_atk", 3.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::DANMAK_END_3), {
		notify{ L"em3000_skirt_close_short", 48.f, [playSound]() { playSound(L"em3000_skirt_close_short"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::PHASE_CHANGE_1), {
		notify{ L"em3000_skirt_open", 0.f, [playSound]() { playSound(L"em3000_skirt_open"); } },
		notify{ L"em3000_vo_m_atk", 21.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
		notify{ L"em3000_walk_start", 21.f, [playSound]() { playSound(L"em3000_walk_start"); } },
		notify{ L"em3000_step", 49.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 49.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_bite_neck_appear", 96.f, [playSound]() { playSound(L"em3000_bite_neck_appear"); } },
		notify{ L"em3000_bite_move", 123.f, [playSound]() { playSound(L"em3000_bite_move"); } },
		notify{ L"em3000_runaway_forward_bite", 135.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_runaway_forward_bite", 207.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_runaway_forward_bite", 242.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_runaway_forward_bite", 272.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_vo_laugh", 272.f, [playVoice]() { playVoice(L"em3000_vo_laugh"); } },
		notify{ L"em3000_walk_end", 399.f, [playSound]() { playSound(L"em3000_walk_end"); } },
		notify{ L"em3000_foot_stomp_end", 440.f, [playSound]() { playSound(L"em3000_foot_stomp_end"); } },
		notify{ L"em3000_skirt_close", 482.f, [playSound]() { playSound(L"em3000_skirt_close"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::PHASE_CHANGE_2), {
		notify{ L"em3000_skirt_open", 0.f, [playSound]() { playSound(L"em3000_skirt_open"); } },
		notify{ L"em3000_vo_m_atk", 21.f, [playVoice]() { playVoice(L"em3000_vo_m_atk"); } },
		notify{ L"em3000_walk_start", 21.f, [playSound]() { playSound(L"em3000_walk_start"); } },
		notify{ L"em3000_step", 49.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 49.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_bite_neck_appear", 96.f, [playSound]() { playSound(L"em3000_bite_neck_appear"); } },
		notify{ L"em3000_bite_move", 123.f, [playSound]() { playSound(L"em3000_bite_move"); } },
		notify{ L"em3000_runaway_forward_bite", 135.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_runaway_forward_bite", 207.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_runaway_forward_bite", 242.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_runaway_forward_bite", 272.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_vo_laugh", 272.f, [playVoice]() { playVoice(L"em3000_vo_laugh"); } },
		notify{ L"em3000_walk_end", 399.f, [playSound]() { playSound(L"em3000_walk_end"); } },
		notify{ L"em3000_foot_stomp_end", 440.f, [playSound]() { playSound(L"em3000_foot_stomp_end"); } },
		notify{ L"em3000_skirt_close", 482.f, [playSound]() { playSound(L"em3000_skirt_close"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::PHASE_CHANGE_3), {
		notify{ L"em3000_runaway_kick", 0.f, [playSound]() { playSound(L"em3000_runaway_kick"); } },
		notify{ L"em3000_vo_shout", 0.f, [playVoice]() { playVoice(L"em3000_vo_shout"); } },
		notify{ L"em3000_step", 43.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 43.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_runaway_kick", 44.f, [playSound]() { playSound(L"em3000_runaway_kick"); } },
		notify{ L"em3000_walk_start", 64.f, [playSound]() { playSound(L"em3000_walk_start"); } },
		notify{ L"em3000_step", 72.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 72.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_runaway_lift", 78.f, [playSound]() { playSound(L"em3000_runaway_lift"); } },
		notify{ L"em3000_runaway_creak", 120.f, [playSound]() { playSound(L"em3000_runaway_creak"); } },
		notify{ L"em3000_runaway_howl", 154.f, [playVoice]() { playVoice(L"em3000_runaway_howl"); } },
		notify{ L"em3000_vo_cry", 180.f, [playVoice]() { playVoice(L"em3000_vo_cry"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::CHASE), {
		notify{ L"em3000_step", 4.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 4.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_runaway_forward_bite", 9.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_step", 19.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 19.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_runaway_forward_bite", 25.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_step", 34.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 34.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_runaway_forward_bite", 42.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_step", 49.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 49.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_runaway_forward_bite", 60.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_step", 64.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 64.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_runaway_forward_bite", 77.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_step", 79.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 79.f, [playSound]() { playSound(L"em3000_step_move"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::READY_CHASE), {
		notify{ L"em3000_step", 19.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 19.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_step", 31.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 31.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_runaway_forward_bite", 75.f, [playSound]() { playSound(L"em3000_runaway_forward_bite"); } },
		notify{ L"em3000_step", 79.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 79.f, [playSound]() { playSound(L"em3000_step_move"); } },
		notify{ L"em3000_step", 84.f, [playSound]() { playSound(L"em3000_step"); } },
		notify{ L"em3000_step_move", 84.f, [playSound]() { playSound(L"em3000_step_move"); } },
	});
	// e001 seq SeTrack (s × 60): the jets light with steam and the skirt opening, hum while it hovers and die on landing.
	m_Model->Add_AnimNotify(ETOI(anim::HOVER_IN), {
		notify{ L"em3000_hover_steam", 20.f, [playSound]() { playSound(L"em3000_hover_steam"); } },
		notify{ L"em3000_skirt_open", 20.f, [playSound]() { playSound(L"em3000_skirt_open"); } },
		notify{ L"em3000_hover_Loop", 20.f, 161.f, [this]() { Play_LoopSound(L"em3000_hover_Loop"); }, [this]() { Stop_LoopSound(); } },
		notify{ L"em3000_walk_start", 146.f, [playSound]() { playSound(L"em3000_walk_start"); } },
		notify{ L"em3000_hover_end", 148.f, [playSound]() { playSound(L"em3000_hover_end"); } },
		notify{ L"em3000_hover_steam", 166.f, [playSound]() { playSound(L"em3000_hover_steam"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::SCREAMING), {
		notify{ L"em3000_vo_shout", 0.f, [playVoice]() { playVoice(L"em3000_vo_shout"); } },
		notify{ L"em3000_runaway_creak", 50.f, [playSound]() { playSound(L"em3000_runaway_creak"); } },
		notify{ L"em3000_runaway_howl", 56.f, [playVoice]() { playVoice(L"em3000_runaway_howl"); } },
	});
	return S_OK;
}

Shared<Em3000Body> Em3000Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em3000Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em3000Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em3000Body::Clone(void* arg)
{
	auto instance = make_shared<Em3000Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em3000Body");
		return nullptr;
	}

	return instance;
}

