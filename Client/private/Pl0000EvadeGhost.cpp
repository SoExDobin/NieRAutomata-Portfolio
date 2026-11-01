#include "pch.h"
#include "Pl0000EvadeGhost.h"

#include <Game.h>
#include <Model.h>
#include <Shader.h>
#include <SpdLogger.h>
#include <VIBuffer_Particle_Point.h>

Pl0000EvadeGhost::Pl0000EvadeGhost() : GameObject{} {}
Pl0000EvadeGhost::Pl0000EvadeGhost(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{device, context} {}
Pl0000EvadeGhost::Pl0000EvadeGhost(const Pl0000EvadeGhost& rhs)
	: GameObject{rhs} {}

HRESULT Pl0000EvadeGhost::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT Pl0000EvadeGhost::Initialize(void* arg)
{
	if (nullptr == arg)
	{
		LOG_ERROR(L"Failed to Initialize Pl0000EvadeGhost : nullptr arg");
		return E_FAIL;
	}

	if (FAILED(GameObject::Initialize(arg)))
		return E_FAIL;

	EVADE_GHOST_DESC& desc = *static_cast<EVADE_GHOST_DESC*>(arg);
	if (!desc.model || !desc.shader)
	{
		LOG_ERROR(L"Pl0000EvadeGhost requires a model and a shader");
		return E_FAIL;
	}

	m_SnapShots = std::move(desc.snapShots);
	m_SpawnWorldMatrix = desc.worldMatrix;
	m_LifeTime = desc.lifeTime;
	m_GhostColor = desc.ghostColor;
	m_RefModel = desc.model;
	m_RefShader = desc.shader;
	m_FollowPose = desc.followPose;
	m_DrawBody = !desc.effectOnly;
	m_Origin = m_SpawnWorldMatrix.Translation();
	m_TravelDirection = desc.travelDirection;
	if (desc.evadeEffect)
	{
		Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag,
			VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
		m_EffectShader = Add_Component<Shader>(ETOI(LEVEL::GAMEPLAY), &shaderDesc);
		if (!m_EffectShader || !m_EffectShader->Has_Pass("Evade"))
			return E_FAIL;

		VIBuffer_Particle_Point::VIBUFFER_INSTANCE_POINT_DESC particles{};
		particles.numInstances = 48;
		particles.isLoop = false;
		particles.center = Vector3{ 0.f, 0.7f, 0.f };
		particles.range = Vector3{ 1.6f, 1.4f, 1.6f };
		particles.scale = Vector2{ 0.18f, 0.58f };
		particles.speed = Vector2{ 0.15f, 0.65f };
		particles.lifeTime = Vector2{ 0.36f, 0.55f };
		m_EffectBuffer = Add_Component<VIBuffer_Particle_Point>(ETOI(LEVEL::GAMEPLAY), &particles);
		if (!m_EffectBuffer)
			return E_FAIL;

		m_EffectWorldMatrix = Matrix::CreateTranslation(m_SpawnWorldMatrix.Translation());
		m_EffectStrength = desc.perfectEvade ? 1.35f : 0.68f;
	}

	return S_OK;
}

void Pl0000EvadeGhost::Update(Float timeDelta)
{
	m_Elapsed += timeDelta;
	if (m_Elapsed >= m_LifeTime)
	{
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}

	const Float progress = std::clamp(m_Elapsed / m_LifeTime, 0.f, 1.f);
	const Float fade = std::clamp((1.f - progress) / 0.7f, 0.f, 1.f);
	m_Alpha = fade * fade * (3.f - 2.f * fade);
	if (m_TravelDirection.LengthSquared() > 0.f)
	{
		m_SpawnWorldMatrix.Translation(m_Origin + m_TravelDirection * (m_TravelDistance * progress));
	}
	if (m_EffectBuffer)
		m_EffectBuffer->Update_Spread(timeDelta);
}

void Pl0000EvadeGhost::Strengthen_PerfectEvade()
{
	if (!m_FollowPose && !m_EffectBuffer) return;
	m_GhostColor = Vector4{ 1.35f, 1.35f, 1.35f, 0.42f };
	m_EffectStrength = 1.35f;
	m_TravelDistance = 3.f;
	m_LifeTime = std::max(m_LifeTime, m_Elapsed + 0.45f);
}

void Pl0000EvadeGhost::Submit_RenderGroup()
{
	if (Is_Active() && (!m_DrawBody || m_RefShader->Has_Pass(GhostPass)))
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::BLEND, shared_from_this());
}

HRESULT Pl0000EvadeGhost::Render()
{
	if (FAILED(m_RefShader->Bind_Matrix(WorldMatrix, &m_SpawnWorldMatrix)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_RefShader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_RefShader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_CameraPosition(m_RefShader, CameraPosition)))
		return E_FAIL;
	if (FAILED(m_RefShader->Bind_RawValue("g_GhostColor", &m_GhostColor, sizeof(Vector4))))
		return E_FAIL;
	if (FAILED(m_RefShader->Bind_RawValue("g_GhostAlpha", &m_Alpha, sizeof(Float))))
		return E_FAIL;

	const Matrix identity = Matrix::Identity;
	const size_t numMeshes = !m_DrawBody ? 0 : m_FollowPose ? m_RefModel->Get_NumMeshes() :
		std::min<size_t>(m_RefModel->Get_NumMeshes(), m_SnapShots.size());
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		string meshName;
		uint32 materialIndex = 0;
		if (SUCCEEDED(m_RefModel->Get_MeshMaterialInfo(i, meshName, materialIndex)) &&
			meshName.starts_with("Broken"))
			continue;
		if (m_FollowPose)
		{
			if (FAILED(m_RefModel->Bind_BoneMatrices(m_RefShader, BoneMatrices, i))) return E_FAIL;
		}
		else
		{
			// A mesh without bones still reads one palette entry, same as Model::Bind_BoneMatrices.
			const BONE_SNAPSHOT& snapShot = m_SnapShots[i];
			if (snapShot.numBones > 0)
				m_RefShader->Bind_Matrices(BoneMatrices, snapShot.matrices, snapShot.numBones);
			else
				m_RefShader->Bind_Matrices(BoneMatrices, &identity, 1);
		}

		if (FAILED(m_RefModel->BindAndBeginMaterial(m_RefShader, i)) ||
			FAILED(m_RefShader->Begin(GhostPass)))
			return E_FAIL;
		if (FAILED(m_RefModel->Render(i)))
			return E_FAIL;
	}
	if (m_EffectBuffer)
	{
		const uint32 lockUpRight = 1;
		if (FAILED(m_EffectShader->Bind_Matrix(WorldMatrix, &m_EffectWorldMatrix)) ||
			FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_EffectShader, ViewMatrix, D3DTS::VIEW)) ||
			FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_EffectShader, ProjMatrix, D3DTS::PROJ)) ||
			FAILED(GAME_INSTANCE->Bind_CameraPosition(m_EffectShader, CameraPosition)) ||
			FAILED(m_EffectShader->Bind_RawValue("g_LockUpRight", &lockUpRight, sizeof(uint32))) ||
			FAILED(m_EffectShader->Bind_RawValue("g_EvadeStrength", &m_EffectStrength, sizeof(Float))) ||
			FAILED(m_EffectShader->Begin("Evade")) ||
			FAILED(m_EffectBuffer->Bind_Resources()) ||
			FAILED(m_EffectBuffer->Render()))
			return E_FAIL;
	}

	return S_OK;
}

Shared<Pl0000EvadeGhost> Pl0000EvadeGhost::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<Pl0000EvadeGhost>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Pl0000EvadeGhost");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> Pl0000EvadeGhost::Clone(void* arg)
{
	auto instance = make_shared<Pl0000EvadeGhost>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : Pl0000EvadeGhost");
		return nullptr;
	}

	return instance;
}
