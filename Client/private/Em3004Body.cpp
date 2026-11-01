#include "pch.h"
#include "Em3004Body.h"

#include <AABBCollider.h>
#include <Game.h>
#include <Model.h>
#include <Shader.h>
#include <SoundEmitter.h>
#include <SpdLogger.h>

#include "Bullet.h"
#include "ClientSettingManager.h"
#include "Em3004.h"
#include "Random_Helper.h"

Em3004Body::Em3004Body() : PartObject{} {}
Em3004Body::Em3004Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{ device, context } {
}
Em3004Body::Em3004Body(const Em3004Body& rhs)
	: PartObject{ rhs } {
}

HRESULT Em3004Body::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Monster");
	m_TagMask.Set_Tag({ L"Monster" });

	return PartObject::Initialize_Prototype();
}

HRESULT Em3004Body::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed To Ready_Components : EM3004Body");
		return E_FAIL;
	}

	if (FAILED(Ready_AnimationNotify()))
	{
		LOG_ERROR(L"Failed To Ready_AnimationNotify : EM3004Body");
		return E_FAIL;
	}

	m_MuzzleBoneIndex = m_Model->Get_BoneIndexByName("bone5");
	if (m_MuzzleBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find em3004 head bone5");
		return E_FAIL;
	}

	Pick_Puppet();
	return S_OK;
}

void Em3004Body::Pick_Puppet()
{
	// And_02 also has And_02.001 and .002 (its extra parts): every mesh whose name starts with the chosen puppet's.
	const string puppet = "And_0" + std::to_string(Helper::Random_Int(0, NumPuppets - 1));
	const uint32 numMeshes = static_cast<uint32>(m_Model->Get_NumMeshes());
	m_Shown.assign(numMeshes, false);
	string meshName;
	uint32 materialIndex{};
	for (uint32 i = 0; i < numMeshes; ++i)
		if (SUCCEEDED(m_Model->Get_MeshMaterialInfo(i, meshName, materialIndex)))
			m_Shown[i] = meshName.starts_with(puppet) || meshName == "Gare_00" || meshName == "Gare_01" || meshName == "Gare_02";
}

void Em3004Body::On_Destroy()
{
	PartObject::On_Destroy();
}

void Em3004Body::Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop)
{
	m_Model->Set_Animation(animIndex, blendDuration);
	m_Model->Set_AnimLoop(isLoop);
}

uint32 Em3004Body::Get_CurrentAnimationIndex() const
{
	return m_Model->Get_AnimationIndex();
}

Bool Em3004Body::Is_AnimationFinished() const
{
	return m_Model->Is_AnimationFinished();
}

Vector3 Em3004Body::Get_Center() const
{
	return Vector3::Transform(Vector3{ 0.f, 2.f, 0.f }, m_CombinedWorldMatrix);
}

void Em3004Body::Off_Colliders() const
{
	m_HitBox->Set_Active(false);
}

Vector3 Em3004Body::Begin_Shatter()
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

	// The shader rests the parts 1 m below the centre: on the floor the puppet stands on.
	m_ShatterTime = 0.f;
	m_ShatterCenter = m_CombinedWorldMatrix.Translation() + Vector3{ 0.f, 1.f, 0.f };
	return m_ShatterCenter;
}

void Em3004Body::Fire_Bullet()
{
	const auto owner = static_pointer_cast<Em3004>(Get_Owner());
	if (!owner || owner->Is_Dead()) return;
	const Shared<GameObject> target = owner->Get_Target();
	if (!target || !owner->Is_TargetFront()) return;

	const Vector3 head = (m_Model->Get_BoneMatrix(m_MuzzleBoneIndex) * m_CombinedWorldMatrix).Translation();
	// Aim at the chest; the target's position is at its feet.
	Vector3 direction = target->Get_Transform()->Get_Position() + Vector3{ 0.f, 1.f, 0.f } - head;
	if (direction.LengthSquared() < 0.0001f) return;
	direction.Normalize();
	const Vector3 muzzle = head + direction * MuzzleReach;

	Bullet::BULLET_DESC desc{};
	desc.damageInfo.attacker = owner;
	desc.damageInfo.attackType = ATK_TYPE::LIGHT;
	desc.damageInfo.damage = owner->Get_ShotDamage();
	desc.damageInfo.groggyWeight = 0.f;
	desc.damageInfo.knockbackForce = 0.f;
	desc.resourceTag = L"candy";
	desc.targetLayer = L"Player";
	desc.isPermanent = false;
	desc.speed = owner->Get_ShotSpeed();
	desc.maxDistance = ShotRange;
	desc.initialPosition = muzzle;
	desc.direction = direction;
	GAME_INSTANCE->Instantiate<Bullet>(L"Bullet", Get_LevelIndex(), &desc);
	++m_ShotCount;

	GAME_INSTANCE->PlaySound3D(L"em3004_default_shot", m_SoundEmitter->Get_Volume(), muzzle,
		m_SoundEmitter->Get_NearDistance(), m_SoundEmitter->Get_FarDistance(), false);
}

void Em3004Body::Priority_Update(Float timeDelta)
{
}

void Em3004Body::Update(Float timeDelta)
{
	if (m_ShatterTime >= 0.f)
	{
		m_ShatterTime += timeDelta;
		return;
	}
	m_Model->Update_ModelAnimation(timeDelta * m_AnimationRate);
}

void Em3004Body::Late_Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(*m_Transform->Get_WorldMatrixPtr());
	m_HitBox->Update(m_CombinedWorldMatrix);
}

void Em3004Body::Fixed_Update(Float fixedDelta)
{

}

HRESULT Em3004Body::Render()
{
	// Flying pieces cast no shadow; the frozen pose would leave a standing shadow behind.
	const Bool shattering = m_ShatterTime >= 0.f;
	if (shattering && GAME_INSTANCE->Is_ShadowPass())
		return S_OK;
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	size_t numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (i < m_Shown.size() && !m_Shown[i])
			continue;
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

void Em3004Body::Submit_RenderGroup()
{
	if (Is_Active())
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

HRESULT Em3004Body::Bind_ShaderResources()
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

HRESULT Em3004Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXANIMMESH::Tag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"em3004" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;
	// Summoned at run time, so no scene document carries a preset snapshot.
	if (m_Model->Get_AnimationPreset().Is_Empty())
	{
		AnimationPresetSnapshot preset;
		if (FAILED(ClientSettingManager::GetInstance()->Load_AnimationPreset(L"em3004", preset)) ||
			FAILED(m_Model->Apply_AnimationPreset(preset)))
			return E_FAIL;
	}

	// The body hangs from its strings with the feet 1.2 m up and the head near 2.9 m.
	AABBCollider::AABB_COLLIDER_DESC aabbDesc{};
	aabbDesc.extents = Vector3{ 0.45f, 0.9f, 0.45f };
	aabbDesc.offset = Vector3{ 0.f, 2.05f, 0.f };
	m_HitBox = Add_Component<AABBCollider>(ETOI(LEVEL::STATIC), &aabbDesc);
	if (nullptr == m_HitBox)
		return E_FAIL;

	SoundEmitter::SOUND_EMITTER_DESC emitterDesc{};
	emitterDesc.volume = 0.3f;
	emitterDesc.nearDistance = 5.f;
	emitterDesc.farDistance = 40.f;
	m_SoundEmitter = Add_Component<SoundEmitter>(ETOI(LEVEL::STATIC), &emitterDesc);
	if (nullptr == m_SoundEmitter)
		return E_FAIL;

	return S_OK;
}

HRESULT Em3004Body::Ready_AnimationNotify()
{
	using anim = Em3004::EM3004_ANIM;
	using notify = AnimationTracker::ANIMATION_NOTIFY;

	// em3004 seq: x1 plays em3004_fall_impact (put on frame 17, where the posed clip lands), x6 fires at 0, 0.25 and
	// 0.5 s of every lap.
	m_Model->Add_AnimNotify(ETOI(anim::FALL), {
		notify{ L"em3004_fall_impact", 17.f, [this]() { m_SoundEmitter->Play(L"em3004_fall_impact"); } },
	});
	m_Model->Add_AnimNotify(ETOI(anim::SHOOT_LOOP), {
		notify{ L"Shot1", 0.f, [this]() { Fire_Bullet(); } },
		notify{ L"Shot2", 15.f, [this]() { Fire_Bullet(); } },
		notify{ L"Shot3", 30.f, [this]() { Fire_Bullet(); } },
	});

	return S_OK;
}

Shared<Em3004Body> Em3004Body::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Em3004Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Em3004Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Em3004Body::Clone(void* arg)
{
	auto instance = make_shared<Em3004Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Em3004Body");
		return nullptr;
	}

	return instance;
}
