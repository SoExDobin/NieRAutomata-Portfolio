#include "pch.h"
#include "Em3000Parts.h"

#include <Game.h>

#include "SoftBody.h"
#include "Model.h"
#include "SpdLogger.h"


Em3000Parts::Em3000Parts() : PartObject{} {}
Em3000Parts::Em3000Parts(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: PartObject{device, context} {}
Em3000Parts::Em3000Parts(const Em3000Parts& rhs)
	: PartObject{rhs} {}

HRESULT Em3000Parts::Initialize_Prototype()
{
	m_LayerMask.Set_Layer(L"Monster");
	m_TagMask.Set_Tag({ L"Monster" });

	return PartObject::Initialize_Prototype();
}

HRESULT Em3000Parts::Initialize(void* arg)
{
	if (nullptr == arg)
	{
		LOG_ERROR(L"Failed to Initialize Em3000Part");
		return E_FAIL;
	}



	auto desc = static_cast<tagEm3000PartsDesc*>(arg);

	if (FAILED(Ready_Components(desc->shaderDesc, desc->modelResourceTag)))
	{
		LOG_ERROR(L"Failed to Ready_Components BodyModel Em3000Parts");
		return E_FAIL;
	}

	m_ParentModel = desc->parentModel;
	if (const Shared<Model> parentModel = m_ParentModel.lock())
	{
		m_ParentBoneIndex = parentModel->Get_BoneIndexByName(desc->parentBoneName);
		m_RootBoneIndex = m_Model->Get_BoneIndexByName(desc->rootBoneName);
		if (m_ParentBoneIndex == -1 || m_RootBoneIndex == -1)
		{
			LOG_ERROR(L"Failed to Find Em3000Parts Parent Bone");
			return E_FAIL;
		}
	}

	return PartObject::Initialize(arg);
}

void Em3000Parts::Begin_Shatter(const Vector3& center)
{
	Update_PartWorldMatrix();
	m_Shatter.Begin(m_Model, center);
}

void Em3000Parts::Update(Float timeDelta)
{
	m_Shatter.Update(timeDelta);
	if (m_BurnTime >= 0.f)
		m_BurnTime += timeDelta;
	if (Get_BurnProgress() >= 1.f || m_Shatter.Is_Gone())
		Set_Active(false);
}

Float Em3000Parts::Get_BurnProgress() const
{
	return m_BurnTime >= 0.f ? std::min(m_BurnTime / BurnDuration, 1.f) : -1.f;
}

void Em3000Parts::Late_Update(Float timeDelta)
{
	// The pieces keep the pose they died in.
	if (m_Shatter.Is_Running())
		return;
	// Back to the bind pose first: last frame's swing is still in the bones (the part has no clip to re-pose them).
	m_Model->Update_ModelAnimation(timeDelta);
	Update_PartWorldMatrix();
	// The colliders are on the boss's body bones (head, shoulders, arms), which share this part's transform.
	m_SoftBody->Simulate(m_Model, m_CombinedWorldMatrix, timeDelta, m_ParentModel.lock(), m_Transform->Get_WorldMatrix());
}

void Em3000Parts::Submit_RenderGroup()
{
	// Also here rather than only in Late_Update so the part follows its bone in edit mode.
	Update_PartWorldMatrix();
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
	// The glowing burn line, added into RT_EMISSIVE.
	if (Get_BurnProgress() >= 0.f)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::EMISSIVE, shared_from_this());
}

void Em3000Parts::Update_PartWorldMatrix()
{
	m_Transform->Update_WorldMatrix();
	if (const Shared<Model> parentModel = m_ParentModel.lock())
		// The part has no animation and is drawn in its bind pose, whose root lies on the parent bone's bind pose. Taking the
		// root back to the origin and onto the parent bone as it is posed now carries the whole part with that bone.
		Update_CombineWorldMatrix(m_Model->Get_BoneMatrix(m_RootBoneIndex).Invert()
			* parentModel->Get_BoneMatrix(m_ParentBoneIndex) * m_Transform->Get_WorldMatrix());
	else
		Update_CombineWorldMatrix(m_Transform->Get_WorldMatrix());
}

HRESULT Em3000Parts::Render()
{
	const Float progress = Get_BurnProgress();
	const Bool burning = progress >= 0.f;
	const Bool shattering = m_Shatter.Is_Running();
	// A burning cloth or flying pieces cast no shadow: the whole one would stay on the floor.
	if ((burning || shattering) && GAME_INSTANCE->Is_ShadowPass())
		return S_OK;
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;
	if (burning && FAILED(m_Shader->Bind_RawValue("g_BurnProgress", &progress, sizeof(Float))))
		return E_FAIL;
	if (shattering && FAILED(m_Shatter.Bind(m_Shader)))
		return E_FAIL;
	const Bool burnEdge = GAME_INSTANCE->Get_DrawingGroup() == RENDERGROUP::EMISSIVE;
	size_t numMeshes = m_Model->Get_NumMeshes();
	for (uint32 i = 0; i < numMeshes; ++i)
	{
		if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i))) return E_FAIL;
		if (burnEdge)
		{
			// The material's albedo alpha keeps the edge off the cloth's cut-out holes.
			if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)) || FAILED(m_Shader->Begin("BurnEdge")) ||
				FAILED(m_Model->Render(i))) return E_FAIL;
			continue;
		}
		if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
			return E_FAIL;
		// Same material textures, re-begun with the burning pixel shader or the part-scatter vertex shader.
		if (burning && FAILED(m_Shader->Begin("Burn")))
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

HRESULT Em3000Parts::Bind_ShaderResources()
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

HRESULT Em3000Parts::Ready_Components(Shader::SHADER_DESC shaderDesc, const wstring& modelResourceTag)
{
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader) return E_FAIL;

	Model::MODEL_DESC modelDesc{ modelResourceTag };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model) return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	m_SoftBody = Add_Component<SoftBody>(ETOI(LEVEL::STATIC));
	if (nullptr == m_SoftBody) return E_FAIL;

	return S_OK;
}
