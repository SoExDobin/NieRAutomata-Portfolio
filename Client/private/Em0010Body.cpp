#include "pch.h"
#include "Em0010Body.h"

#include <AABBCollider.h>
#include <Game.h>
#include <Model.h>
#include <Shader.h>
#include <SoundEmitter.h>
#include <SpdLogger.h>

#include "Bullet.h"
#include "Em0010.h"
#include "EmTelegraph.h"
#include "Firework.h"
#include "MonsterAOE.h"
#include "Entity.h"

Em0010Body::Em0010Body() : PartObject{} {}
Em0010Body::Em0010Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{ device, context}{}
Em0010Body::Em0010Body(const Em0010Body& rhs)
	: PartObject{rhs} {}

HRESULT Em0010Body::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Monster");
	m_TagMask.Set_Tag({ L"Monster" });

	return PartObject::Initialize_Prototype();
}

HRESULT Em0010Body::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : Em0010Body");
		return E_FAIL;
	}

	if (FAILED(Ready_AnimationNotify()))
	{
		LOG_ERROR(L"Failed To Ready_AnimationNotify : Em0010Body");
		return E_FAIL;
	}

	m_RootBoneIndex = m_Model->Get_BoneIndexByName("em0010");

	if (m_RootBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find Em0010 Root Bone");
		return E_FAIL;
	}

	m_Model->Set_LocalRootNode(m_RootBoneIndex);

	// em0010_Gun's gun arm (arm_parts1) is skinned to bone306 only; every em0010 variant shares the skeleton.
	m_GunBoneIndex = m_Model->Get_BoneIndexByName("bone306");

	return S_OK;
}

HRESULT Em0010Body::Begin()
{
	m_LeftArm = static_pointer_cast<MonsterAOE>(Get_Owner()->Find_Child(L"Em0010LeftArm"));
	if (m_LeftArm.expired())
		return E_FAIL;
	m_LeftArm.lock()->DeActive_Attack();

	m_RightArm = static_pointer_cast<MonsterAOE>(Get_Owner()->Find_Child(L"Em0010RightArm"));
	if (m_RightArm.expired())
		return E_FAIL;
	m_RightArm.lock()->DeActive_Attack();

	m_Foot = static_pointer_cast<MonsterAOE>(Get_Owner()->Find_Child(L"Em0010Foot"));
	if (m_Foot.expired())
		return E_FAIL;
	m_Foot.lock()->DeActive_Attack();

	return S_OK;
}

void Em0010Body::On_Destroy()
{
	PartObject::On_Destroy();
}

const TRANSFORM_FRAME& Em0010Body::Get_ModelTransform() const
{
	return m_Model->Get_RootTransformVelocity(m_RootBoneIndex);
}

void Em0010Body::Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop)
{
	m_Model->Set_Animation(animIndex, blendDuration);
	m_Model->Set_AnimLoop(isLoop);
}

uint32 Em0010Body::Get_CurrentAnimationIndex() const
{
	return m_Model->Get_AnimationIndex();
}

uint32 Em0010Body::Get_NextAnimationIndex() const
{
	return m_Model->Get_NextAnimationIndex();
}

Float Em0010Body::Get_AnimationProgress() const
{
	return m_Model->Get_AnimationProgress();
}

Bool Em0010Body::Is_AnimationFinished() const
{
	return m_Model->Is_AnimationFinished();
}

void Em0010Body::Fire_Bullet(const Vector3& aimPosition)
{
	if (m_GunBoneIndex == -1) return;

	// The barrel reaches about 0.85 m past bone306 (rest-pose mesh extent) and the shoot motion points it at the target.
	constexpr Float barrelLength = 0.85f;
	const Vector3 gunPosition = (m_Model->Get_BoneMatrix(m_GunBoneIndex) * m_CombinedWorldMatrix).Translation();
	Vector3 direction = aimPosition - gunPosition;
	if (direction.LengthSquared() < 0.0001f) return;
	direction.Normalize();

	// Small chip damage: below the player's HIT threshold (30), em0010EmInfo04 machine gun is REACTION_SMALL.
	Entity::DAMAGE_INFO dmgInfo{};
	dmgInfo.attacker = Get_Owner();
	dmgInfo.attackType = ATK_TYPE::LIGHT;
	dmgInfo.damage = 12.f;
	dmgInfo.groggyWeight = 0.f;
	dmgInfo.knockbackForce = 0.f;

	Bullet::BULLET_DESC desc{};
	desc.damageInfo = dmgInfo;
	desc.resourceTag = L"candy";
	desc.targetLayer = L"Player";
	desc.isPermanent = false;
	desc.speed = 20.f;		// em0010AtkInfo04 SPD_20
	desc.maxDistance = 30.f;	// beyond the 20 m shooting range
	desc.initialPosition = gunPosition + direction * barrelLength;
	desc.direction = direction;

	GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_19);
	// 원작 AtkInfo의 SE(wp0001_MachineGun_Shot)는 어느 뱅크에도 없어, 적 탄 발사음(core_hak_em_shot)을 쓴다.
	GAME_INSTANCE->PlaySoundFXOnce(L"core_hak_em_shot", SOUNDCHANNEL::CHANNEL_19, 0.3f, &gunPosition);
	GAME_INSTANCE->Instantiate<Bullet>(L"Bullet", Get_LevelIndex(), &desc);
}

void Em0010Body::Fire_Firework()
{
	if (m_GunBoneIndex == -1) return;

	Shared<Firework> sky;
	for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(Get_LevelIndex()))
		if ((sky = Cast<Firework>(object)))
			break;
	if (!sky) return;

	// Up past the barrel's end (0.85 m from bone306), leaning a little the way the machine faces.
	const Vector3 gunPosition = (m_Model->Get_BoneMatrix(m_GunBoneIndex) * m_CombinedWorldMatrix).Translation();
	Vector3 forward = Get_Owner()->Get_Transform()->Get_Look();
	forward.y = 0.f;
	forward.Normalize();
	sky->Launch(gunPosition + Vector3{ 0.f, 0.85f, 0.f }, Vector3{ 0.f, 1.f, 0.f } + forward * 0.25f);

	GAME_INSTANCE->StopSound(SOUNDCHANNEL::CHANNEL_19);
	GAME_INSTANCE->PlaySoundFXOnce(L"core_hak_em_shot", SOUNDCHANNEL::CHANNEL_19, 0.3f, &gunPosition);
}

void Em0010Body::Off_Colliders() const
{
	m_HitBox->Set_Active(false);
	for (const auto& aoe : { m_LeftArm, m_RightArm, m_Foot })
		if (const auto attack = aoe.lock())
			attack->DeActive_Attack();
}

Vector3 Em0010Body::Begin_Shatter()
{
	if (m_ShatterTime >= 0.f) return m_ShatterCenter;

	// Each part pivots on the centre of the vertices its bone dominates.
	constexpr size_t maxParts = 256;
	const uint32 numMeshes = static_cast<uint32>(m_Model->Get_NumMeshes());
	m_PartCenters.assign(numMeshes, {});
	vector<Vector3> centers;
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (!m_Model->Get_MeshPartCenters(i, centers) || centers.size() > maxParts)
			continue;
		for (const Vector3& center : centers)
			m_PartCenters[i].emplace_back(center.x, center.y, center.z, 1.f);
	}

	// Parts fly out from the middle of the body (the hit box spans 0..2 m above the feet).
	m_ShatterTime = 0.f;
	m_SoundEmitter->Play(L"em_common_death_bom_m");	// the bosses' death explosion
	m_ShatterCenter = m_CombinedWorldMatrix.Translation() + Vector3{ 0.f, 1.f, 0.f };
	return m_ShatterCenter;
}

void Em0010Body::Priority_Update(Float timeDelta)
{

}

void Em0010Body::Update(Float timeDelta)
{
	EmTelegraph::Advance_Tell(m_TellAge, timeDelta);
	if (m_CrackTime >= 0.f)
		m_CrackTime += timeDelta;

	if (m_ShatterTime >= 0.f)
	{
		m_ShatterTime += timeDelta;
		return;
	}

	m_Model->Update_ModelAnimation(timeDelta);
}

void Em0010Body::Late_Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	m_HitBox->Update(m_CombinedWorldMatrix);
}

void Em0010Body::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT Em0010Body::Render()
{
	if (GAME_INSTANCE->Get_DrawingGroup() == RENDERGROUP::WORLDUI)
		return Render_Crack();
	if (GAME_INSTANCE->Get_DrawingGroup() == RENDERGROUP::EMISSIVE)
		return FAILED(Bind_ShaderResources()) ? E_FAIL : EmTelegraph::Render_Eyes(m_Model, m_Shader, m_TellAge);

	// Flying pieces cast no shadow; the frozen pose would leave a standing shadow behind.
	const Bool shattering = m_ShatterTime >= 0.f;
	if (shattering && GAME_INSTANCE->Is_ShadowPass())
		return S_OK;

	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	size_t numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i))) return E_FAIL;

		if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
			return E_FAIL;
		// Same material textures, re-begun with the part-scatter vertex shader.
		if (shattering)
		{
			const auto& centers = m_PartCenters[i];
			if (centers.empty())
				continue;
			if (FAILED(m_Shader->Bind_RawValue("g_PartCenters", centers.data(), static_cast<uint32>(centers.size() * sizeof(Vector4)))) ||
				FAILED(m_Shader->Bind_RawValue("g_ShatterSeed", &i, sizeof(uint32))) ||
				FAILED(m_Shader->Begin("Shatter")))
				return E_FAIL;
		}

		if (FAILED(m_Model->Render(i))) return E_FAIL;
	}

	return S_OK;
}

void Em0010Body::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
	// Additive fissures after the transparent map pass, while the body is still whole.
	if (m_CrackTime >= 0.f && m_ShatterTime < 0.f)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::WORLDUI, shared_from_this());
	// The eyes glow until it starts to die.
	const Shared<Entity> owner = Cast<Entity>(Get_Owner());
	if (owner && !owner->Is_Dead() && m_CrackTime < 0.f && m_ShatterTime < 0.f)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::EMISSIVE, shared_from_this());
}

HRESULT Em0010Body::Render_Crack()
{
	const Float progress = std::min(m_CrackTime / n_CrackSpreadTime, 1.f);
	if (FAILED(Bind_ShaderResources()) ||
		FAILED(m_Shader->Bind_RawValue("g_CrackProgress", &progress, sizeof(Float))) ||
		FAILED(m_Shader->Bind_RawValue("g_CrackTime", &m_CrackTime, sizeof(Float))))
		return E_FAIL;

	const size_t numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i)) ||
			FAILED(m_Shader->Begin("Crack")) ||
			FAILED(m_Model->Render(i)))
			return E_FAIL;
	}

	return S_OK;
}

void Em0010Body::OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void Em0010Body::OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

void Em0010Body::OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider)
{
	
}

HRESULT Em0010Body::Bind_ShaderResources()
{
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_CombinedWorldMatrix)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	if (m_ShatterTime >= 0.f &&
		(FAILED(m_Shader->Bind_RawValue("g_ShatterTime", &m_ShatterTime, sizeof(Float))) ||
		 FAILED(m_Shader->Bind_RawValue("g_ShatterDuration", &ShatterDuration, sizeof(Float))) ||
		 FAILED(m_Shader->Bind_RawValue("g_ShatterCenter", &m_ShatterCenter, sizeof(Vector3)))))
		return E_FAIL;

	return S_OK;
}

HRESULT Em0010Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXANIMMESH::Tag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"em0010" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	AABBCollider::AABB_COLLIDER_DESC aabbDesc{};
	aabbDesc.extents = Vector3{ 1.f, 1.f, 1.f };
	aabbDesc.offset = Vector3{ 0.f, 1.f, 0.f };;
	SoundEmitter::SOUND_EMITTER_DESC emitterDesc{};
	emitterDesc.volume = 0.3f;
	emitterDesc.nearDistance = 5.f;
	emitterDesc.farDistance = 40.f;
	m_SoundEmitter = Add_Component<SoundEmitter>(ETOI(LEVEL::STATIC), &emitterDesc);
	if (nullptr == m_SoundEmitter)
		return E_FAIL;

	m_HitBox = Add_Component<AABBCollider>(ETOI(LEVEL::STATIC), &aabbDesc);
	if (nullptr == m_HitBox)
		return E_FAIL;

	return S_OK;
}

HRESULT Em0010Body::Ready_AnimationNotify()
{
	using em0010State = Em0010::EM0010_STATE;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	// 원작 SeTrack 사운드 이벤트는 몸의 SoundEmitter로 낸다(카메라와의 거리만큼 줄어든다).
	auto playSound = [this](const tChar* tag) { m_SoundEmitter->Play(tag); };
	auto telegraph = [this]() {
		EmTelegraph::Play(static_pointer_cast<PartObject>(shared_from_this()), m_Model);
		m_TellAge = 0.f;
	};

	auto activeRightArm = [this]() { m_RightArm.lock()->Active_Attack(); };
	auto deActiveRightArm = [this]() { m_RightArm.lock()->DeActive_Attack(); };

	auto activeLeftArm = [this]() { m_LeftArm.lock()->Active_Attack(); };
	auto deActiveLeftArm = [this]() { m_LeftArm.lock()->DeActive_Attack(); };

	auto activeFoot = [this]() { m_Foot.lock()->Active_Attack(); };
	auto deActiveFoot = [this]() { m_Foot.lock()->DeActive_Attack(); };

	m_Model->Add_AnimNotify(ETOI(em0010State::SWING_FRONT), {
		notify{ L"em0010_atk_move_charge", 1.f, [playSound]() { playSound(L"em0010_atk_move_charge"); } },
		notify{ L"em0010_punch_wsh", 55.f, [playSound]() { playSound(L"em0010_punch_wsh"); } },
		notify{ L"em0010_step_walk_R", 76.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_L", 89.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_atk_move_end_short", 121.f, [playSound]() { playSound(L"em0010_atk_move_end_short"); } },
		notify{ L"em0010_step_walk_servo", 158.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 181.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{L"Telegraph", 0.f, telegraph},
		notify{L"Swing", 70.f, 80.f, activeRightArm, deActiveRightArm}
		});
	m_Model->Add_AnimNotify(ETOI(em0010State::PUNCH_FRONT), {
		notify{ L"em0010_atk_move_charge", 1.f, [playSound]() { playSound(L"em0010_atk_move_charge"); } },
		notify{ L"em0010_step_walk_R", 38.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_run_L", 61.f, [playSound]() { playSound(L"em0010_step_run_L"); } },
		notify{ L"em0010_punch_wsh", 71.f, [playSound]() { playSound(L"em0010_punch_wsh"); } },
		notify{ L"em0010_move_end_servo", 105.f, [playSound]() { playSound(L"em0010_move_end_servo"); } },
		notify{ L"em0010_atk_move_end_long", 116.f, [playSound]() { playSound(L"em0010_atk_move_end_long"); } },
		notify{ L"em0010_step_walk_servo", 150.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_R", 181.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{L"Telegraph", 0.f, telegraph},
		notify{L"Punch", 80.f, 90.f, activeLeftArm, deActiveLeftArm}
		});
	m_Model->Add_AnimNotify(ETOI(em0010State::FOOT_ATTACK), {
		notify{ L"em0010_stomp_start", 1.f, [playSound]() { playSound(L"em0010_stomp_start"); } },
		notify{ L"em0010_stomp_impact", 53.f, [playSound]() { playSound(L"em0010_stomp_impact"); } },
		notify{ L"em0010_atk_move_end_short", 99.f, [playSound]() { playSound(L"em0010_atk_move_end_short"); } },
		notify{ L"em0010_step_walk_servo", 127.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 153.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{L"Telegraph", 0.f, telegraph},
		notify{L"Foot", 60.f, 65.f, activeFoot, deActiveFoot}
		});
	m_Model->Add_AnimNotify(ETOI(em0010State::SWING_TWICE), {
		notify{ L"em0010_atk_move_charge", 1.f, [playSound]() { playSound(L"em0010_atk_move_charge"); } },
		notify{ L"em0010_step_walk_L", 24.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_punch_wsh", 68.f, [playSound]() { playSound(L"em0010_punch_wsh"); } },
		notify{ L"em0010_step_walk_R", 82.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_punch_wsh", 106.f, [playSound]() { playSound(L"em0010_punch_wsh"); } },
		notify{ L"em0010_step_walk_L", 118.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_atk_move_end_short", 157.f, [playSound]() { playSound(L"em0010_atk_move_end_short"); } },
		notify{ L"em0010_step_walk_servo", 195.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_R", 224.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{L"Telegraph", 0.f, telegraph},
		notify{L"LeftSwing", 75.f, 85.f, activeLeftArm, deActiveLeftArm},
		notify{L"RightSwing", 120.f, 130.f, activeRightArm, deActiveRightArm},
		});

	m_Model->Add_AnimNotify(ETOI(em0010State::WALK_F), {
		notify{ L"em0010_step_walk_servo", 1.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_R", 37.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_servo", 39.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 76.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::WALK_B), {
		notify{ L"em0010_step_walk_R", 7.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_servo", 10.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 46.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_step_walk_servo", 48.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::WALK_L), {
		notify{ L"em0010_step_walk_servo", 1.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_R", 35.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_servo", 40.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_servo", 69.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 78.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_step_walk_servo", 81.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_R", 100.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::WALK_R), {
		notify{ L"em0010_step_walk_servo", 1.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 35.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_step_walk_servo", 40.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_servo", 69.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_R", 78.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_servo", 81.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 100.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::WALK_LOOK_TARGET_L), {
		notify{ L"em0010_step_run_servo", 15.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_servo", 31.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_run_servo", 46.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_R", 56.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_run_servo", 69.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_L", 80.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_step_run_servo", 91.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_R", 101.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_run_servo", 110.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_L", 118.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_step_walk_R", 137.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_servo", 147.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 172.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::WALK_LOOK_TARGET_R), {
		notify{ L"em0010_step_run_servo", 15.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_servo", 31.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_run_servo", 46.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_L", 56.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_step_run_servo", 69.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_R", 80.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_run_servo", 91.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_L", 101.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_step_run_servo", 110.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_R", 118.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_R", 137.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_servo", 147.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_R", 172.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::WALK_END), {
		notify{ L"em0010_step_walk_servo", 1.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_R", 37.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::SPRINT_1), {
		notify{ L"em0010_step_run_servo", 1.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_R", 18.f, [playSound]() { playSound(L"em0010_step_run_R"); } },
		notify{ L"em0010_step_run_servo", 25.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_L", 42.f, [playSound]() { playSound(L"em0010_step_run_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::SPRINT_2), {
		notify{ L"em0010_step_run_servo", 1.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_R", 18.f, [playSound]() { playSound(L"em0010_step_run_R"); } },
		notify{ L"em0010_step_run_servo", 25.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_L", 42.f, [playSound]() { playSound(L"em0010_step_run_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::START_SPRINT_1), {
		notify{ L"em0010_step_walk_servo", 1.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_run_servo", 6.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_walk_servo", 10.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_run_R", 36.f, [playSound]() { playSound(L"em0010_step_run_R"); } },
		notify{ L"em0010_step_run_servo", 50.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_L", 62.f, [playSound]() { playSound(L"em0010_step_run_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::START_SPRINT_2), {
		notify{ L"em0010_step_walk_servo", 1.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 20.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
		notify{ L"em0010_step_run_servo", 26.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_R", 47.f, [playSound]() { playSound(L"em0010_step_run_R"); } },
		notify{ L"em0010_step_run_servo", 55.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_L", 64.f, [playSound]() { playSound(L"em0010_step_run_L"); } },
		notify{ L"em0010_step_run_servo", 73.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_R", 82.f, [playSound]() { playSound(L"em0010_step_run_R"); } },
		notify{ L"em0010_step_run_servo", 91.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_L", 107.f, [playSound]() { playSound(L"em0010_step_run_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::START_SPRINT_3), {
		notify{ L"em0010_step_run_servo", 0.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_R", 25.f, [playSound]() { playSound(L"em0010_step_run_R"); } },
		notify{ L"em0010_step_run_servo", 39.f, [playSound]() { playSound(L"em0010_step_run_servo"); } },
		notify{ L"em0010_step_run_L", 52.f, [playSound]() { playSound(L"em0010_step_run_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::SPRINT_END), {
		notify{ L"em0010_step_walk_R", 9.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
		notify{ L"em0010_step_walk_servo", 13.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 30.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::ESCAPE_BACK), {
		notify{ L"em0010_avoid", 1.f, [playSound]() { playSound(L"em0010_avoid"); } },
		notify{ L"em0010_jump_land", 28.f, [playSound]() { playSound(L"em0010_jump_land"); } },
		notify{ L"em0010_step_walk_L", 81.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::HIT_FRONT_1), {
		notify{ L"em_common_dmg_forly_short_mid", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_short_mid"); } },
		notify{ L"em0010_step_walk_L", 24.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::HIT_FRONT_2), {
		notify{ L"em_common_dmg_forly_short_mid", 0.f, [playSound]() { playSound(L"em_common_dmg_forly_short_mid"); } },
		notify{ L"em0010_step_walk_L", 19.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::HIT_FRONT_3), {
		notify{ L"em_common_dmg_forly_short_mid", 0.f, [playSound]() { playSound(L"em_common_dmg_forly_short_mid"); } },
		notify{ L"em0010_step_walk_R", 30.f, [playSound]() { playSound(L"em0010_step_walk_R"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::HIT_BACK_1), {
		notify{ L"em_common_dmg_forly_short_mid", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_short_mid"); } },
		notify{ L"em0010_step_walk_L", 24.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::HIT_BACK_2), {
		notify{ L"em_common_dmg_forly_short_mid", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_short_mid"); } },
		notify{ L"em0010_step_walk_L", 24.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::HIT_BACK_3), {
		notify{ L"em_common_dmg_forly_short_mid", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_short_mid"); } },
		notify{ L"em0010_step_walk_L", 24.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::BACK_DROP_1), {
		notify{ L"em_common_dmg_forly_long_mid", 0.f, [playSound]() { playSound(L"em_common_dmg_forly_long_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::BACK_DROP_3), {
		notify{ L"em_common_s_down_mid", 4.f, [playSound]() { playSound(L"em_common_s_down_mid"); } },
		notify{ L"em_common_down_bound_mid", 29.f, [playSound]() { playSound(L"em_common_down_bound_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::AIR_LAUNCH), {
		notify{ L"em_common_dmg_forly_long_mid", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_long_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::AIR_LAND), {
		notify{ L"em_common_s_down_mid", 4.f, [playSound]() { playSound(L"em_common_s_down_mid"); } },
		notify{ L"em_common_down_bound_mid", 29.f, [playSound]() { playSound(L"em_common_down_bound_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::AIR_REHIT), {
		notify{ L"em_common_dmg_forly_long_mid", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_long_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::AIR_RECOVER), {
		notify{ L"em_common_recover_mid", 0.f, [playSound]() { playSound(L"em_common_recover_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::DEAD_1), {
		notify{ L"em0010_dizzy", 1.f, [playSound]() { playSound(L"em0010_dizzy"); } },
		notify{ L"em_common_s_down_mid", 57.f, [playSound]() { playSound(L"em_common_s_down_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::DEAD_2), {
		notify{ L"em0010_dizzy", 1.f, [playSound]() { playSound(L"em0010_dizzy"); } },
		notify{ L"em_common_s_down_mid", 61.f, [playSound]() { playSound(L"em_common_s_down_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::DEAD_3), {
		notify{ L"em0010_dizzy_start", 1.f, [playSound]() { playSound(L"em0010_dizzy_start"); } },
		notify{ L"em_common_down_bound_mid", 25.f, [playSound]() { playSound(L"em_common_down_bound_mid"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::JUMP_TO_1), {
		notify{ L"em0010_jump_move", 0.f, [playSound]() { playSound(L"em0010_jump_move"); } },
		notify{ L"em0010_jump_wsh", 7.f, [playSound]() { playSound(L"em0010_jump_wsh"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::JUMP_TO_3), {
		notify{ L"em0010_jump_stomp_impact", 0.f, [playSound]() { playSound(L"em0010_jump_stomp_impact"); } },
		notify{ L"em0010_atk_move_end_short", 40.f, [playSound]() { playSound(L"em0010_atk_move_end_short"); } },
		notify{ L"em0010_step_walk_servo", 77.f, [playSound]() { playSound(L"em0010_step_walk_servo"); } },
		notify{ L"em0010_step_walk_L", 100.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::SHOOT_START), {
		notify{ L"em0010_atk_move_gun_start", 1.f, [playSound]() { playSound(L"em0010_atk_move_gun_start"); } },
		notify{ L"em0010_step_walk_L", 19.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0010State::SHOOT_END), {
		notify{ L"em0010_atk_move_gun_end", 1.f, [playSound]() { playSound(L"em0010_atk_move_gun_end"); } },
		notify{ L"em0010_step_walk_L", 60.f, [playSound]() { playSound(L"em0010_step_walk_L"); } },
	});
	return S_OK;
}

Shared<Em0010Body> Em0010Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em0010Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em0010Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em0010Body::Clone(void* arg)
{
	auto instance = make_shared<Em0010Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em0010Body");
		return nullptr;
	}

	return instance;
}


