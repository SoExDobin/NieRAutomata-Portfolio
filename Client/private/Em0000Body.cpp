#include "pch.h"
#include "Em0000Body.h"

#include <AABBCollider.h>
#include <Game.h>
#include <Model.h>
#include <Shader.h>
#include <SoundEmitter.h>
#include <SpdLogger.h>

#include "ConfettiEffect.h"
#include "Em0000.h"
#include "EmTelegraph.h"
#include "MonsterAOE.h"

Em0000Body::Em0000Body() : PartObject{} {}
Em0000Body::Em0000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{ device, context } {}
Em0000Body::Em0000Body(const Em0000Body& rhs)
	: PartObject{ rhs } {}

HRESULT Em0000Body::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Monster");
	m_TagMask.Set_Tag({ L"Monster" });

	return PartObject::Initialize_Prototype();
}

HRESULT Em0000Body::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : Em0000Body");
		return E_FAIL;
	}

	if (FAILED(Ready_AnimationNotify()))
	{
		LOG_ERROR(L"Failed To Ready_AnimationNotify : Em0000Body");
		return E_FAIL;
	}

	// Every em000x mesh is converted with its armature named after the family, so the root motion bone is em0000.
	m_RootBoneIndex = m_Model->Get_BoneIndexByName("em0000");
	if (m_RootBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find Em0000 Root Bone");
		return E_FAIL;
	}
	m_Model->Set_LocalRootNode(m_RootBoneIndex);

	return S_OK;
}

HRESULT Em0000Body::Begin()
{
	const wstring& prefix = static_pointer_cast<Em0000>(Get_Owner())->Get_PartPrefix();
	const auto find = [this, &prefix](const tChar* part, Weak<MonsterAOE>& out) {
		out = static_pointer_cast<MonsterAOE>(Get_Owner()->Find_Child(prefix + part));
		if (out.expired())
			return false;
		out.lock()->DeActive_Attack();
		return true;
	};
	if (!find(L"LeftArm", m_LeftArm) || !find(L"RightArm", m_RightArm) || !find(L"Torso", m_Torso))
		return E_FAIL;

	return S_OK;
}

void Em0000Body::On_Destroy()
{
	PartObject::On_Destroy();
}

const TRANSFORM_FRAME& Em0000Body::Get_ModelTransform() const
{
	return m_Model->Get_RootTransformVelocity(m_RootBoneIndex);
}

void Em0000Body::Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop)
{
	m_Model->Set_Animation(animIndex, blendDuration);
	m_Model->Set_AnimLoop(isLoop);
}

uint32 Em0000Body::Get_CurrentAnimationIndex() const
{
	return m_Model->Get_AnimationIndex();
}

Bool Em0000Body::Is_AnimationFinished() const
{
	return m_Model->Is_AnimationFinished();
}

Float Em0000Body::Get_AnimationProgress() const
{
	return m_Model->Get_AnimationProgress();
}

void Em0000Body::Telegraph()
{
	EmTelegraph::Play(static_pointer_cast<PartObject>(shared_from_this()), m_Model, Get_Owner()->Get_Transform()->Get_Scale().x,
		L"Em_Tele", EyeReach);
	m_TellAge = 0.f;
}

Bool Em0000Body::Is_EyeLit() const
{
	const auto owner = static_pointer_cast<Em0000>(Get_Owner());
	return owner && !owner->Is_Dormant() && !owner->Is_Dead() && m_CrackTime < 0.f && m_ShatterTime < 0.f;
}

void Em0000Body::Off_Colliders() const
{
	m_HitBox->Set_Active(false);
	for (const auto& aoe : { m_LeftArm, m_RightArm, m_Torso })
		if (const auto attack = aoe.lock())
			attack->DeActive_Attack();
}

Vector3 Em0000Body::Begin_Shatter()
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

	// Parts fly out from the middle of the body (the hit box spans 0..1.4 m above the feet).
	m_ShatterTime = 0.f;
	m_SoundEmitter->Play(L"em_common_death_bom_m");	// the bosses' death explosion
	m_ShatterCenter = m_CombinedWorldMatrix.Translation() + Vector3{ 0.f, 0.7f, 0.f };
	return m_ShatterCenter;
}

void Em0000Body::Priority_Update(Float timeDelta)
{
}

void Em0000Body::Update(Float timeDelta)
{
	EmTelegraph::Advance_Tell(m_TellAge, timeDelta);
	if (m_CrackTime >= 0.f)
		m_CrackTime += timeDelta;

	if (m_ShatterTime >= 0.f)
	{
		m_ShatterTime += timeDelta;
		return;
	}

	// A body scaled up in the scene (Emb001) moves at the slower cadence of a bigger build: time grows with the square
	// root of size, so its strides cover the scaled distance (Em0010Movement) at sqrt(scale) times the speed.
	const auto owner = static_pointer_cast<Em0000>(Get_Owner());
	m_Model->Update_ModelAnimation(timeDelta / sqrtf(owner->Get_Transform()->Get_Scale().x) * owner->Get_AnimationRate());
}

void Em0000Body::Late_Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	m_HitBox->Update(m_CombinedWorldMatrix);
}

void Em0000Body::Fixed_Update(Float fixedDelta)
{
}

HRESULT Em0000Body::Render()
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

	const size_t numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i)) ||
			FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
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

void Em0000Body::Submit_RenderGroup()
{
	// Also runs in edit mode, where Late_Update does not: keep the mesh on the owner's transform.
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
	// Additive fissures after the transparent map pass, while the body is still whole.
	if (m_CrackTime >= 0.f && m_ShatterTime < 0.f)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::WORLDUI, shared_from_this());
	if (Is_EyeLit())
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::EMISSIVE, shared_from_this());
}

HRESULT Em0000Body::Render_Crack()
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

HRESULT Em0000Body::Bind_ShaderResources()
{
	if (FAILED(m_Shader->Bind_Matrix(WorldMatrix, &m_CombinedWorldMatrix)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	if (m_ShatterTime >= 0.f &&
		(FAILED(m_Shader->Bind_RawValue("g_ShatterTime", &m_ShatterTime, sizeof(Float))) ||
		 FAILED(m_Shader->Bind_RawValue("g_ShatterDuration", &ShatterDuration, sizeof(Float))) ||
		 FAILED(m_Shader->Bind_RawValue("g_ShatterCenter", &m_ShatterCenter, sizeof(Vector3)))))
		return E_FAIL;

	return S_OK;
}

HRESULT Em0000Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXANIMMESH::Tag, VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// Level 4 answers em0000 with the amusement-park em0003; the scene's Model ModelTag can name another em000x.
	Model::MODEL_DESC modelDesc{ Get_ModelTag() };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	SoundEmitter::SOUND_EMITTER_DESC emitterDesc{};
	emitterDesc.volume = 0.3f;
	emitterDesc.nearDistance = 5.f;
	emitterDesc.farDistance = 40.f;
	m_SoundEmitter = Add_Component<SoundEmitter>(ETOI(LEVEL::STATIC), &emitterDesc);
	if (nullptr == m_SoundEmitter)
		return E_FAIL;

	AABBCollider::AABB_COLLIDER_DESC aabbDesc{};
	aabbDesc.extents = Vector3{ 0.6f, 0.7f, 0.6f };
	aabbDesc.offset = Vector3{ 0.f, 0.7f, 0.f };
	m_HitBox = Add_Component<AABBCollider>(ETOI(LEVEL::STATIC), &aabbDesc);
	if (nullptr == m_HitBox)
		return E_FAIL;

	return S_OK;
}

HRESULT Em0000Body::Ready_AnimationNotify()
{
	using em0000State = Em0000::EM0000_STATE;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	// 원작 SeTrack 사운드 이벤트(em0000.bnk)는 몸의 SoundEmitter로 낸다. em0000_vo_head_atk·vo_punch는 어느 뱅크에도 없어(원작도 무음) 뺐다.
	auto playSound = [this](const tChar* tag) { m_SoundEmitter->Play(tag); };

	Ready_MoveNotify();

	for (const em0000State hit : { em0000State::HIT_FRONT, em0000State::HIT_BACK })
		m_Model->Add_AnimNotify(ETOI(hit), notify{ L"em_common_dmg_forly_short_s", 0.f,
			[playSound]() { playSound(L"em_common_dmg_forly_short_s"); } });
	m_Model->Add_AnimNotify(ETOI(em0000State::HIT_BIG_FRONT), {
		notify{ L"em_common_dmg_forly_long_s", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_long_s"); } },
		notify{ L"em0000_step_move", 83.f, [playSound]() { playSound(L"em0000_step_move"); } },
		notify{ L"em0000_step_L", 103.f, [playSound]() { playSound(L"em0000_step_L"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::HIT_BIG_BACK), {
		notify{ L"em_common_dmg_forly_long_s", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_long_s"); } },
		notify{ L"em0000_step_move", 69.f, [playSound]() { playSound(L"em0000_step_move"); } },
		notify{ L"em0000_step_R", 102.f, [playSound]() { playSound(L"em0000_step_R"); } },
	});
	for (const em0000State launch : { em0000State::BLOW_START, em0000State::AIR_LAUNCH, em0000State::AIR_REHIT })
		m_Model->Add_AnimNotify(ETOI(launch), notify{ L"em_common_dmg_forly_long_s", 0.f,
			[playSound]() { playSound(L"em_common_dmg_forly_long_s"); } });
	m_Model->Add_AnimNotify(ETOI(em0000State::BLOW_LAND), notify{ L"em_common_s_down_s", 0.f,
		[playSound]() { playSound(L"em_common_s_down_s"); } });
	m_Model->Add_AnimNotify(ETOI(em0000State::AIR_LAND), notify{ L"em_common_l_down_s", 0.f,
		[playSound]() { playSound(L"em_common_l_down_s"); } });
	m_Model->Add_AnimNotify(ETOI(em0000State::RECOVER), {
		notify{ L"em_common_dmg_forly_long_s", 0.f, [playSound]() { playSound(L"em_common_dmg_forly_long_s"); } },
		notify{ L"em_common_recover_front_s", 20.f, [playSound]() { playSound(L"em_common_recover_front_s"); } },
		notify{ L"em0000_jump_land", 43.f, [playSound]() { playSound(L"em0000_jump_land"); } },
		notify{ L"em0000_step_move", 81.f, [playSound]() { playSound(L"em0000_step_move"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::DEAD), {
		notify{ L"em_common_dmg_forly_long_s", 1.f, [playSound]() { playSound(L"em_common_dmg_forly_long_s"); } },
		notify{ L"em_common_l_down_s", 40.f, [playSound]() { playSound(L"em_common_l_down_s"); } },
	});

	// e060, the park visitor's paper throw (Level 4 em0003): em000f.eff 200 scatters texture 125 from the hands at frames
	// 30 and 160, and the seq SeTrack rolls the paper out with em0000.bnk / plffff.bnk sounds.
	auto confetti = [this]() { Throw_Confetti(); };
	m_Model->Add_AnimNotify(ETOI(em0000State::PAPER_THROW), {
		notify{ L"em0000_paper_throw_roll", 25.f, [playSound]() { playSound(L"em0000_paper_throw_roll"); } },
		notify{ L"em_common_paper", 28.f, [playSound]() { playSound(L"em_common_paper"); } },
		notify{ L"Confetti", 30.f, confetti },
		notify{ L"em0000_paper_normal", 146.f, [playSound]() { playSound(L"em0000_paper_normal"); } },
		notify{ L"em_common_paper", 155.f, [playSound]() { playSound(L"em_common_paper"); } },
		notify{ L"Confetti", 160.f, confetti },
	});

	Ready_AttackNotify();
	return S_OK;
}

void Em0000Body::Throw_Confetti()
{
	const Shared<Transform> owner = Get_Owner()->Get_Transform();
	const Float scale = owner->Get_Scale().x;
	Vector3 forward = owner->Get_Look();
	forward.y = 0.f;
	forward.Normalize();

	// Out of the raised hands (about 1.3 m up, a little in front), thrown up and forward.
	ConfettiEffect::CONFETTI_EFFECT_DESC desc{};
	desc.position = owner->Get_Position() + Vector3{ 0.f, 1.3f * scale, 0.f } + forward * 0.3f * scale;
	desc.throwDirection = Vector3{ 0.f, 0.8f, 0.f } + forward * 0.6f;
	desc.floorHeight = owner->Get_Position().y;
	GAME_INSTANCE->Instantiate<ConfettiEffect>(L"ConfettiEffect", Get_LevelIndex(), &desc);
}

void Em0000Body::Ready_MoveNotify()
{
	using em0000State = Em0000::EM0000_STATE;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	auto playSound = [this](const tChar* tag) { m_SoundEmitter->Play(tag); };
	m_Model->Add_AnimNotify(ETOI(em0000State::WALK_F), {
		notify{ L"em0000_step_R", 14.f, [playSound]() { playSound(L"em0000_step_R"); } },
		notify{ L"em0000_step_move", 25.f, [playSound]() { playSound(L"em0000_step_move"); } },
		notify{ L"em0000_step_L", 44.f, [playSound]() { playSound(L"em0000_step_L"); } },
		notify{ L"em0000_step_move", 55.f, [playSound]() { playSound(L"em0000_step_move"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::DASH), {
		notify{ L"em0000_jump", 1.f, [playSound]() { playSound(L"em0000_jump"); } },
		notify{ L"em0000_jump_land", 42.f, [playSound]() { playSound(L"em0000_jump_land"); } },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::ESCAPE_BACK), {
		notify{ L"em0000_avoid", 0.f, [playSound]() { playSound(L"em0000_avoid"); } },
		notify{ L"em0000_jump_land", 21.f, [playSound]() { playSound(L"em0000_jump_land"); } },
	});
}

void Em0000Body::Ready_AttackNotify()
{
	using em0000State = Em0000::EM0000_STATE;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	auto playSound = [this](const tChar* tag) { m_SoundEmitter->Play(tag); };
	auto telegraph = [this]() { Telegraph(); };
	auto armsOn = [this]() { m_LeftArm.lock()->Active_Attack(); m_RightArm.lock()->Active_Attack(); };
	auto armsOff = [this]() { m_LeftArm.lock()->DeActive_Attack(); m_RightArm.lock()->DeActive_Attack(); };
	auto torsoOn = [this]() { m_Torso.lock()->Active_Attack(); };
	auto torsoOff = [this]() { m_Torso.lock()->DeActive_Attack(); };

	// Strike windows bracket the fastest hand / chest frames of each clip (hands peak at 61 in 0500 and 72 in 0510,
	// the chest drives down from 23 in 0520 and the whole body slides in 0521).
	m_Model->Add_AnimNotify(ETOI(em0000State::PUNCH), {
		notify{ L"em0000_head_atk", 35.f, [playSound]() { playSound(L"em0000_head_atk"); } },
		notify{ L"em0000_jump_land", 76.f, [playSound]() { playSound(L"em0000_jump_land"); } },
		notify{ L"em0000_step_move", 81.f, [playSound]() { playSound(L"em0000_step_move"); } },
		notify{ L"Telegraph", 0.f, telegraph },
		notify{ L"Punch", 55.f, 75.f, armsOn, armsOff },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::SWING_LUNGE), {
		notify{ L"em0000_dash_atk_start", 14.f, [playSound]() { playSound(L"em0000_dash_atk_start"); } },
		notify{ L"em0000_punch", 38.f, [playSound]() { playSound(L"em0000_punch"); } },
		notify{ L"em0000_jump_land", 77.f, [playSound]() { playSound(L"em0000_jump_land"); } },
		notify{ L"Telegraph", 0.f, telegraph },
		notify{ L"Swing", 62.f, 85.f, armsOn, armsOff },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::TACKLE_START), {
		notify{ L"em0000_unique_rocket_omen", 0.f, [playSound]() { playSound(L"em0000_unique_rocket_omen"); } },
		notify{ L"em0000_unique_rocket_atk", 59.f, [playSound]() { playSound(L"em0000_unique_rocket_atk"); } },
		notify{ L"Telegraph", 0.f, telegraph },
		notify{ L"Tackle", 40.f, 64.f, torsoOn, torsoOff },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::TACKLE_SLIDE), {
		notify{ L"Slide", 0.f, 10.f, torsoOn, torsoOff },
	});
	m_Model->Add_AnimNotify(ETOI(em0000State::TACKLE_GETUP), {
		notify{ L"em0000_jump", 0.f, [playSound]() { playSound(L"em0000_jump"); } },
		notify{ L"em0000_unique_rocket_end", 0.f, [playSound]() { playSound(L"em0000_unique_rocket_end"); } },
		notify{ L"em0000_jump_land", 58.f, [playSound]() { playSound(L"em0000_jump_land"); } },
	});
}

Shared<Em0000Body> Em0000Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em0000Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em0000Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em0000Body::Clone(void* arg)
{
	auto instance = make_shared<Em0000Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em0000Body");
		return nullptr;
	}

	return instance;
}
