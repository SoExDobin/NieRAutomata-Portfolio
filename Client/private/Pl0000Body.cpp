#include "pch.h"
#include "Pl0000Body.h"

#include <SpdLogger.h>

#include "Game.h"

#include "AABBCollider.h"
#include "SoftBody.h"
#include "Entity.h"
#include "Pl0000.h"
#include "Pl0000EvadeGhost.h"
#include "Pl0000Movement.h"
#include "WorldMap.h"

Pl0000Body::Pl0000Body() : Pl0000Parts{} {}
Pl0000Body::Pl0000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Pl0000Parts{device, context} {}
Pl0000Body::Pl0000Body(const Pl0000Body& rhs)
	: Pl0000Parts{rhs} {}

HRESULT Pl0000Body::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Player");
	m_TagMask.Set_Tag({ L"Player" });

	return PartObject::Initialize_Prototype();
}

HRESULT Pl0000Body::Initialize(void* arg)
{
	if (FAILED(Pl0000Parts::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : Pl0000Body");
		return E_FAIL;
	}

	if (FAILED(Ready_AnimationNotify()))
	{
		LOG_ERROR(L"Failed To Ready_AnimationNotify : Pl0000Body");
		return E_FAIL;
	}

	m_RootBoneIndex = m_Model->Get_BoneIndexByName("pl0000");
	
	if (m_RootBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find 2B Root Bone");
		return E_FAIL;
	}

	m_Model->Set_LocalRootNode(m_RootBoneIndex);

	return S_OK;
}

void Pl0000Body::On_Destroy()
{
	PartObject::On_Destroy();
}

void Pl0000Body::Priority_Update(Float timeDelta)
{

}

void Pl0000Body::Update(Float timeDelta)
{
	m_PendingFootsteps.clear();
	m_Model->Update_ModelAnimation(timeDelta);
}

void Pl0000Body::Late_Update(Float timeDelta)
{
	Play_PendingFootsteps();
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	// 치마·머리: 이번 포즈와 몸의 자리가 정해진 뒤에 흔들어 그리기 전에 본으로 넣는다.
	m_SoftBody->Simulate(m_Model, m_CombinedWorldMatrix, timeDelta);
	m_HitBox->Update(m_CombinedWorldMatrix);
}

void Pl0000Body::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT Pl0000Body::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	size_t numMeshes = m_Model->Get_NumMeshes();
	string meshName{};
	uint32 materialIndex{};
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		// Broken* are the damaged outfit (self-destruct / death) that replaces Skirt* and Feather in place; not used yet.
		if (SUCCEEDED(m_Model->Get_MeshMaterialInfo(i, meshName, materialIndex)) && meshName.starts_with("Broken"))
			continue;

		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i))) return E_FAIL;

		if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
			return E_FAIL;

		if (FAILED(m_Model->Render(i))) return E_FAIL;
	}

	return S_OK;
}

void Pl0000Body::Submit_RenderGroup()
{
	// Also runs in edit mode, where Late_Update does not: keep the mesh on the owner's transform.
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	if (Is_Active())
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

Shared<Pl0000EvadeGhost> Pl0000Body::Spawn_Ghost(Float lifeTime, const Vector4& color, Bool evadeEffect, Bool perfectEvade)
{
	Pl0000EvadeGhost::EVADE_GHOST_DESC desc{};
	desc.snapShots = m_Model->Get_SnapShot_BoneMatrices();
	desc.worldMatrix = m_CombinedWorldMatrix;
	desc.lifeTime = lifeTime;
	desc.ghostColor = color;
	desc.evadeEffect = evadeEffect;
	desc.perfectEvade = perfectEvade;
	desc.model = m_Model;
	desc.shader = m_Shader;
	return GAME_INSTANCE->Instantiate<Pl0000EvadeGhost>(L"Pl0000EvadeGhost", Get_LevelIndex(), &desc);
}

Shared<Pl0000EvadeGhost> Pl0000Body::Spawn_EvadeStreaks()
{
	Pl0000EvadeGhost::EVADE_GHOST_DESC desc{};
	desc.worldMatrix = m_CombinedWorldMatrix;
	desc.lifeTime = 0.55f;
	desc.evadeEffect = true;
	desc.effectOnly = true;
	desc.model = m_Model;
	desc.shader = m_Shader;
	return GAME_INSTANCE->Instantiate<Pl0000EvadeGhost>(L"Pl0000EvadeGhost", Get_LevelIndex(), &desc);
}

vector<Shared<Pl0000EvadeGhost>> Pl0000Body::Spawn_EvadeCross(Float lifeTime, const Vector4& color)
{
	vector<Shared<Pl0000EvadeGhost>> ghosts;
	ghosts.reserve(4);
	const Vector3 localDirections[4] = { Vector3::UnitX, -Vector3::UnitX, Vector3::UnitZ, -Vector3::UnitZ };
	for (const Vector3& localDirection : localDirections)
	{
		Pl0000EvadeGhost::EVADE_GHOST_DESC desc{};
		desc.worldMatrix = m_CombinedWorldMatrix;
		desc.lifeTime = lifeTime;
		desc.ghostColor = color;
		desc.followPose = true;
		desc.travelDirection = Vector3::TransformNormal(localDirection, m_CombinedWorldMatrix);
		desc.travelDirection.y = 0.f;
		desc.travelDirection.Normalize();
		desc.model = m_Model;
		desc.shader = m_Shader;
		ghosts.push_back(GAME_INSTANCE->Instantiate<Pl0000EvadeGhost>(L"Pl0000EvadeGhost", Get_LevelIndex(), &desc));
	}
	return ghosts;
}

HRESULT Pl0000Body::Bind_ShaderResources()
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

HRESULT Pl0000Body::Ready_Components()
{

	Shader::SHADER_DESC shaderDesc{ VTXANIMMESH::Tag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	// The City of Ruins (Level 3) plays 2B; the amusement park stage (Level 4) plays A2 (pl0100).
	// A2 is exported with 2B's skeleton names and root node "pl0000", so the 2B animations and root motion apply.
	Model::MODEL_DESC modelDesc{ Get_LevelIndex() == ETOI(LEVEL::AMUSEMENT_PARK) ? L"pl0100" : L"pl0000" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	AABBCollider::AABB_COLLIDER_DESC colDesc{};
	colDesc.extents = Vector3{ 0.5f, 1.f, 0.5f };
	colDesc.offset = Vector3::UnitY;
	m_HitBox = Add_Component<AABBCollider>(ETOI(LEVEL::STATIC), &colDesc);

	m_SoftBody = Add_Component<SoftBody>(ETOI(LEVEL::STATIC));
	if (nullptr == m_SoftBody)
		return E_FAIL;

	return S_OK;
}

void Pl0000Body::Play_PendingFootsteps()
{
	if (m_PendingFootsteps.empty()) return;
	const auto owner = Get_Owner();
	const auto movement = owner ? owner->Get_Component<Pl0000Movement>() : nullptr;
	using state = Pl0000::PL0000_STATE;
	const auto locomotion = [](uint32 animation) {
		switch (static_cast<state>(animation))
		{
		case state::WALK_TO_RUN: case state::RUN_CYCLE: case state::RUN_STOP_L: case state::RUN_STOP_R:
		case state::RUN_LIGHT_CYCLE: case state::RUN_HEAVY_CYCLE: case state::SPRINT_CYCLE: case state::SPRINT_STOP_R:
			return true;
		default: return false;
		}
	};
	const uint32 animation = m_Model->Is_Blending() ? m_Model->Get_NextAnimationIndex() : m_Model->Get_AnimationIndex();
	auto water = WorldMap::FOOTSTEP_WATER::NONE;
	if (movement && movement->Is_Grounded() && locomotion(animation))
	{
		if (!m_WaterTilesGathered)
		{
			m_WaterTilesGathered = true;
			for (const auto& [id, object] : GAME_INSTANCE->Get_GameObjects(Get_LevelIndex()))
			{
				if (const auto tile = Cast<WorldMap>(object); tile && tile->Has_Water())
					m_WaterTiles.push_back(tile);
			}
		}
		const Vector3 foot = owner->Get_Transform()->Get_Position();
		for (const auto& weakTile : m_WaterTiles)
		{
			const auto tile = weakTile.lock();
			if (!tile || tile->Is_Destroy() || !tile->Is_Active()) continue;
			water = tile->Get_FootstepWaterAt(foot);
			if (water != WorldMap::FOOTSTEP_WATER::NONE) break;
		}
	}
	for (const auto& dryTag : m_PendingFootsteps)
	{
		wstring wetTag;
		if (water != WorldMap::FOOTSTEP_WATER::NONE)
		{
			if (dryTag == L"pl0000_step_walk_L" || dryTag == L"pl0000_step_walk_No_att_L") wetTag = L"pl0000_step_water_walk_L";
			else if (dryTag == L"pl0000_step_walk_R" || dryTag == L"pl0000_step_walk_No_att_R") wetTag = L"pl0000_step_water_walk_R";
			else if (dryTag == L"pl0000_step_run_L") wetTag = L"pl0000_step_water_run_L";
			else if (dryTag == L"pl0000_step_run_R") wetTag = L"pl0000_step_water_run_R";
		}
		if (!wetTag.empty() && water == WorldMap::FOOTSTEP_WATER::SEWER)
			wetTag.replace(wetTag.find(L"_water_"), 7, L"_sewer_");
		const Bool useWet = !wetTag.empty() && GAME_INSTANCE->Is_SoundLoaded(wetTag);
		if (!wetTag.empty() && !useWet)
		{
			static Bool reportedMissing = false;
			if (!reportedMissing) { LOG_WARN(L"[Footstep] Missing water preset {}; using dry step", wetTag); reportedMissing = true; }
		}
#ifdef _DEBUG
		if (useWet)
		{
			static uint32 reportedSurfaces = 0;
			const uint32 bit = 1u << static_cast<uint32>(water);
			if (!(reportedSurfaces & bit))
			{
				const Vector3 foot = owner->Get_Transform()->Get_Position();
				LOG_INFO(L"[Footstep] {} at ({:.2f}, {:.2f}, {:.2f})", wetTag, foot.x, foot.y, foot.z);
				reportedSurfaces |= bit;
			}
		}
#endif
		GAME_INSTANCE->PlaySoundFX(useWet ? wetTag : dryTag, SOUNDCHANNEL::CHANNEL_28, 0.2f);
	}
	m_PendingFootsteps.clear();
}

HRESULT Pl0000Body::Ready_AnimationNotify()
{
	using state = Pl0000::PL0000_STATE;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	// 원작 SeTrack 사운드: 발소리(pl0000_step_*), 음성(_vo_), 옷 스침·점프·착지 등.
	// Foot contacts are queued until movement finishes; locomotion selects water or dry variants. Evade/charge stays in each State.
	auto playStep = [this](const tChar* tag) { m_PendingFootsteps.emplace_back(tag); };
	auto playFoley = [](const tChar* tag) { GAME_INSTANCE->PlaySoundFX(tag, SOUNDCHANNEL::CHANNEL_30, 0.25f); };
	// Level 4는 A2(pl0100)다: 같은 음성 이벤트의 A2 목소리(pl0100_vo_*)를 낸다.
	const Bool isA2 = Get_LevelIndex() == ETOI(LEVEL::AMUSEMENT_PARK);
	auto playVoice = [isA2](const tChar* tag) { GAME_INSTANCE->PlaySoundFX(isA2 ? L"pl0100" + wstring(tag + 6) : wstring(tag), SOUNDCHANNEL::CHANNEL_29, 0.3f); };

	m_Model->Add_AnimNotify(ETOI(state::IDLE), {
		notify{ L"pl0000_com_forly_move_end", 14.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_move_end", 86.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_R", 97.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_step_walk_L", 141.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::ATTACK_GROUND), {
		notify{ L"pl0000_com_forly_move_end", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_move_end", 79.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_move_end", 179.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_move_end", 232.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_No_att_L", 264.f, [playStep]() { playStep(L"pl0000_step_walk_No_att_L"); } },
		notify{ L"pl0000_step_walk_No_att_R", 303.f, [playStep]() { playStep(L"pl0000_step_walk_No_att_R"); } },
		notify{ L"pl0000_step_walk_No_att_L", 355.f, [playStep]() { playStep(L"pl0000_step_walk_No_att_L"); } },
		notify{ L"pl0000_step_walk_No_att_R", 465.f, [playStep]() { playStep(L"pl0000_step_walk_No_att_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::ATTACK_AIR), {
		notify{ L"pl0000_step_walk_No_att_R", 47.f, [playStep]() { playStep(L"pl0000_step_walk_No_att_R"); } },
		notify{ L"pl0000_step_walk_No_att_L", 73.f, [playStep]() { playStep(L"pl0000_step_walk_No_att_L"); } },
		notify{ L"pl0000_com_forly_move_end", 121.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_move_end", 192.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_move_end", 290.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::WALK_TO_RUN), {
		notify{ L"pl0000_step_run_R", 21.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 37.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::RUN_CYCLE), {
		notify{ L"pl0000_step_run_R", 13.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 30.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::RUN_STOP_L), {
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_R", 5.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 10.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::RUN_STOP_R), {
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_R", 5.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 10.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::RUN_LIGHT_CYCLE), {
		notify{ L"pl0000_step_run_R", 13.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 30.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::RUN_HEAVY_CYCLE), {
		notify{ L"pl0000_step_run_R", 13.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 30.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::SPRINT_CYCLE), {
		notify{ L"pl0000_step_run_R", 7.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 20.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::SPRINT_STOP_R), {
		notify{ L"pl0000_run_end_iron", 1.f, [playFoley]() { playFoley(L"pl0000_run_end_iron"); } },
		notify{ L"pl0000_com_forly_move_end", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_R", 5.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 10.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DASH_TO_STAND_F), {
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 12.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_walk_R", 51.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DASH_TO_STAND_B), {
		notify{ L"pl0000_com_forly_move_end", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_R", 12.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_step_walk_L", 48.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DASH_TO_STAND_R), {
		notify{ L"pl0000_com_forly_move_end", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_R", 12.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_walk_L", 44.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DASH_TO_STAND_L), {
		notify{ L"pl0000_com_forly_move_end", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 9.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_walk_R", 38.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DASH_F_TO_SPRINT), {
		notify{ L"pl0000_step_run_R", 10.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 14.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DASH_B_TO_SPRINT), {
		notify{ L"pl0000_step_run_L", 13.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 20.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DASH_R_TO_SPRINT), {
		notify{ L"pl0000_step_run_L", 10.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 15.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DASH_L_TO_SPRINT), {
		notify{ L"pl0000_step_run_L", 14.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::EVADE_FRONT), {
		notify{ L"pl0000_com_forly_atk_roll", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_step_run_L", 33.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 48.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_walk_R", 90.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::EVADE_BACKWARD), {
		notify{ L"pl0000_com_forly_atk_roll", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_step_run_R", 25.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 34.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 54.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::EVADE_RIGHT), {
		notify{ L"pl0000_com_forly_atk_roll", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_step_run_L", 29.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 44.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_R", 79.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::EVADE_LEFT), {
		notify{ L"pl0000_com_forly_atk_roll", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_step_run_L", 29.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 44.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_R", 79.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DOUBLE_JUMP1), {
		notify{ L"pl0000_jump_second", 0.f, [playFoley]() { playFoley(L"pl0000_jump_second"); } },
		notify{ L"pl0000_com_forly_move_short", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DOUBLE_JUMP2), {
		notify{ L"pl0000_jump_second", 0.f, [playFoley]() { playFoley(L"pl0000_jump_second"); } },
		notify{ L"pl0000_com_forly_move_short", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DOUBLE_JUMP3), {
		notify{ L"pl0000_jump_second", 0.f, [playFoley]() { playFoley(L"pl0000_jump_second"); } },
		notify{ L"pl0000_com_forly_move_short", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::DOUBLE_JUMP4), {
		notify{ L"pl0000_jump_second", 0.f, [playFoley]() { playFoley(L"pl0000_jump_second"); } },
		notify{ L"pl0000_com_forly_move_short", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::STAND_TO_JUMP), {
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_jump_first", 2.f, [playFoley]() { playFoley(L"pl0000_jump_first"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::RUN_TO_JUMP), {
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_jump_first", 2.f, [playFoley]() { playFoley(L"pl0000_jump_first"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::SPRINT_TO_JUMP), {
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_vo_run_act", 1.f, [playVoice]() { playVoice(L"pl0000_vo_run_act"); } },
		notify{ L"pl0000_jump_first", 2.f, [playFoley]() { playFoley(L"pl0000_jump_first"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::JUMP_TO_RUN), {
		notify{ L"pl0000_step_run_L", 2.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 21.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 35.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_L", 52.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::JUMP_TO_SPRINT), {
		notify{ L"pl0000_land_roll", 1.f, [playFoley]() { playFoley(L"pl0000_land_roll"); } },
		notify{ L"pl0000_step_dash_R", 38.f, [playStep]() { playStep(L"pl0000_step_dash_R"); } },
		notify{ L"pl0000_step_dash_L", 51.f, [playStep]() { playStep(L"pl0000_step_dash_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::JUMP_TO_STAND), {
		notify{ L"pl0000_step_run_L", 1.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_move_end", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_R", 2.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_walk_R", 100.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::POD_GRAB), {
		notify{ L"core_pl_pod_catch", 18.f, [playFoley]() { playFoley(L"core_pl_pod_catch"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::POD_LASER_FIRE), {
		notify{ L"pl0000_com_forly_move_short", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_step_run_R", 14.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_vo_m_atk", 23.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_step_walk_R", 92.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_step_walk_L", 139.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND1), {
		notify{ L"pl0000_com_forly_atk_short", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_vo_l_atk", 4.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_step_run_R", 10.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 12.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_move_end", 92.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 134.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND2), {
		notify{ L"pl0000_vo_l_atk", 4.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_atk_short", 6.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_L", 6.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 14.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 80.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 104.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND3), {
		notify{ L"pl0000_step_run_L", 6.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_vo_l_atk", 7.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_atk_short", 8.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_R", 18.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 22.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 28.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 51.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_walk_L", 187.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
		notify{ L"pl0000_step_walk_R", 243.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND4), {
		notify{ L"pl0000_com_forly_move_short", 4.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_step_run_R", 6.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_vo_l_atk", 25.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_atk_short", 28.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_R", 34.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 36.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_move_end", 161.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 201.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND5), {
		notify{ L"pl0000_com_forly_move_short", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_vo_l_atk", 21.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_step_run_L", 27.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_short", 29.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_R", 42.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 59.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 71.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_walk_R", 85.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_step_walk_R", 146.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND6), {
		notify{ L"pl0000_com_forly_move_short", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_step_run_R", 9.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 16.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_long", 17.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 17.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_vo_m_atk", 17.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_step_run_R", 30.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 31.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_move_end", 48.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 73.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 74.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_walk_R", 114.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND7), {
		notify{ L"pl0000_com_forly_atk_long", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_vo_m_atk", 1.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_com_forly_move_end", 28.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 46.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 48.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 71.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_R", 144.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND_HOLD), {
		notify{ L"pl0000_com_forly_atk_short", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_L", 1.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_roll", 22.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_com_forly_atk_long", 48.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_vo_m_atk", 51.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_step_run_R", 56.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 118.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 176.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_GROUND_RUN), {
		notify{ L"pl0000_com_forly_move_short", 0.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_vo_l_atk", 0.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_step_run_R", 3.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_atk_short", 5.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_com_forly_move_end", 15.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 16.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 17.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 103.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 145.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_AIR1), {
		notify{ L"pl0000_com_forly_atk_short", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_vo_l_atk", 9.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_move_end", 37.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_AIR2), {
		notify{ L"pl0000_com_forly_atk_short", 6.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_vo_l_atk", 12.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_move_end", 50.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_AIR3), {
		notify{ L"pl0000_com_forly_atk_short", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_vo_l_atk", 9.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_move_end", 37.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_AIR4), {
		notify{ L"pl0000_com_forly_atk_short", 6.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_vo_l_atk", 12.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_move_end", 50.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_AIR5), {
		notify{ L"pl0000_com_forly_atk_short", 8.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_vo_m_atk", 13.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_com_forly_move_end", 22.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_AIR_DOWN_ENTER), {
		notify{ L"pl0000_com_forly_atk_long", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_vo_m_atk", 25.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 30.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_AIR_DOWN_END), {
		notify{ L"pl0000_step_run_L", 0.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 1.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 115.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_GROUND1), {
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_atk_roll", 25.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_vo_l_atk", 40.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_step_run_R", 53.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_atk_long", 55.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_step_run_L", 56.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 63.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 135.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 184.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
		notify{ L"pl0000_step_walk_R", 230.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_GROUND2), {
		notify{ L"pl0000_com_forly_move_end", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_atk_roll", 22.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_step_walk_R", 35.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_com_forly_move_short", 37.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_step_run_L", 52.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_long", 54.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_vo_l_atk", 56.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 61.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_step_run_R", 82.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 126.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 155.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
		notify{ L"pl0000_step_walk_R", 239.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_GROUND3), {
		notify{ L"pl0000_com_forly_move_end", 5.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_move_short", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_vo_m_charge", 11.f, [playVoice]() { playVoice(L"pl0000_vo_m_charge"); } },
		notify{ L"pl0000_com_forly_atk_roll", 37.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_step_run_R", 59.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_atk_short", 64.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_L", 70.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_long", 75.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 75.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_step_run_R", 104.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 107.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_move_end", 151.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_com_forly_move_end", 278.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_R", 284.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_GROUND_HOLD_UNFULL), {
		notify{ L"pl0000_com_forly_move_short", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_com_forly_atk_long", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 10.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_vo_m_atk", 19.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_com_forly_move_end", 166.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 198.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_GROUND_HOLD_NO_CONTACT), {
		notify{ L"pl0000_com_forly_move_short", 0.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"core_pl_charge_activity", 4.f, [playFoley]() { playFoley(L"core_pl_charge_activity"); } },
		notify{ L"pl0000_com_forly_atk_short", 11.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_com_forly_atk_long", 15.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_vo_m_atk", 21.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_step_run_L", 29.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 30.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_com_forly_atk_long", 34.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_step_run_R", 40.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 51.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 80.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 111.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 151.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
		notify{ L"pl0000_step_walk_R", 246.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_GROUND_HOLD_FULL), {
		notify{ L"pl0000_com_forly_atk_short", 5.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_L", 6.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 7.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_atk_short", 16.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_L", 24.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_short", 27.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_R", 31.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 35.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_vo_m_atk", 39.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_com_forly_atk_short", 41.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_short"); } },
		notify{ L"pl0000_step_run_R", 41.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 112.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 138.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_GROUND_SPRINT), {
		notify{ L"pl0000_com_forly_move_end", 2.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 5.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_roll", 12.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_step_run_R", 28.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_vo_m_atk", 30.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_com_forly_atk_long", 35.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_step_run_R", 44.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 46.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_com_forly_move_end", 63.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 64.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 73.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 125.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_R", 166.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_AIR_DOWN_ENTER), {
		notify{ L"pl0000_com_forly_move_roll", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_roll"); } },
		notify{ L"pl0000_vo_m_atk", 20.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_com_forly_atk_long", 34.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 34.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_AIR_DOWN_END), {
		notify{ L"pl0000_step_run_R", 0.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 1.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_move_end", 111.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_R", 201.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_step_walk_L", 246.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
		notify{ L"pl0000_step_walk_L", 319.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_LIGHT_GROUND1), {
		notify{ L"pl0000_com_forly_move_short", 0.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_step_run_L", 1.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_vo_l_atk", 9.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 25.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_step_run_L", 29.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 30.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 37.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 109.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
		notify{ L"pl0000_com_forly_move_end", 133.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_LIGHT_GROUND2), {
		notify{ L"pl0000_com_forly_move_short", 0.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_vo_l_atk", 9.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_step_run_L", 10.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 25.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_com_forly_move_end", 37.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_R", 40.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 41.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_walk_R", 108.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_com_forly_move_end", 136.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_R", 181.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_LIGHT_GROUND3), {
		notify{ L"pl0000_com_forly_atk_long_wsh", 5.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_vo_l_atk", 9.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_step_run_R", 12.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 20.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_move_end", 37.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 39.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_long", 44.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_step_run_R", 44.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 49.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_step_run_R", 67.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 68.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_walk_L", 207.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
		notify{ L"pl0000_com_forly_move_end", 211.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_LIGHT_GROUND4), {
		notify{ L"pl0000_com_forly_atk_long", 0.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_step_run_R", 1.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_R", 14.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_L", 18.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_com_forly_atk_long", 36.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 36.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_vo_m_atk", 39.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_step_run_L", 44.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 46.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 199.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_R", 246.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_step_walk_R", 306.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
		notify{ L"pl0000_step_walk_L", 333.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HEAVY_AIR_UP), {
		notify{ L"pl0000_com_forly_move_end", 7.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_vo_m_atk", 12.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_com_forly_atk_long", 28.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_com_forly_atk_long_wsh", 28.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long_wsh"); } },
		notify{ L"pl0000_com_forly_move_roll", 39.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_roll"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::LIGHT_HEAVY_COMBO), {
		notify{ L"pl0000_com_forly_move_end", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_run_L", 13.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_vo_l_atk", 15.f, [playVoice]() { playVoice(L"pl0000_vo_l_atk"); } },
		notify{ L"pl0000_com_forly_atk_roll", 25.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_roll"); } },
		notify{ L"pl0000_step_run_L", 28.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 29.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_run_R", 45.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_atk_long", 55.f, [playFoley]() { playFoley(L"pl0000_com_forly_atk_long"); } },
		notify{ L"pl0000_vo_m_atk", 73.f, [playVoice]() { playVoice(L"pl0000_vo_m_atk"); } },
		notify{ L"pl0000_step_run_L", 75.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 80.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_com_forly_move_end", 135.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_end"); } },
		notify{ L"pl0000_step_walk_L", 183.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
		notify{ L"pl0000_step_walk_R", 234.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HIT1), {
		notify{ L"pl0000_com_forly_move_short", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_vo_l_dmg", 3.f, [playVoice]() { playVoice(L"pl0000_vo_l_dmg"); } },
		notify{ L"pl0000_step_run_L", 15.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 27.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_walk_L", 53.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HIT2), {
		notify{ L"pl0000_com_forly_move_short", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_vo_l_dmg", 3.f, [playVoice]() { playVoice(L"pl0000_vo_l_dmg"); } },
		notify{ L"pl0000_step_run_L", 24.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_walk_R", 66.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HIT3), {
		notify{ L"pl0000_com_forly_move_short", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_vo_l_dmg", 3.f, [playVoice]() { playVoice(L"pl0000_vo_l_dmg"); } },
		notify{ L"pl0000_step_run_L", 27.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_walk_R", 95.f, [playStep]() { playStep(L"pl0000_step_walk_R"); } },
	});

	m_Model->Add_AnimNotify(ETOI(state::HIT4), {
		notify{ L"pl0000_com_forly_move_short", 1.f, [playFoley]() { playFoley(L"pl0000_com_forly_move_short"); } },
		notify{ L"pl0000_vo_l_dmg", 3.f, [playVoice]() { playVoice(L"pl0000_vo_l_dmg"); } },
		notify{ L"pl0000_step_run_L", 19.f, [playStep]() { playStep(L"pl0000_step_run_L"); } },
		notify{ L"pl0000_step_run_R", 40.f, [playStep]() { playStep(L"pl0000_step_run_R"); } },
		notify{ L"pl0000_step_walk_L", 77.f, [playStep]() { playStep(L"pl0000_step_walk_L"); } },
	});

	return S_OK;
}

Shared<Pl0000Body> Pl0000Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Pl0000Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Pl0000Body::Clone(void* arg)
{
	auto instance = make_shared<Pl0000Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000Body");
		return nullptr;
	}

	return instance;
}


