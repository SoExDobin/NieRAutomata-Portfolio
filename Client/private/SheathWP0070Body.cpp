#include "pch.h"
#include "SheathWP0070Body.h"

#include <SpdLogger.h>

#include "Model.h"
#include "SoftBody.h"
#include "Shader.h"
#include "Game.h"


SheathWP0070Body::SheathWP0070Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Pl0000Parts{device, context} {}
SheathWP0070Body::SheathWP0070Body(const SheathWP0070Body& rhs)
	: Pl0000Parts{rhs} {}

HRESULT SheathWP0070Body::Initialize_Prototype()
{
	return Pl0000Parts::Initialize_Prototype();
}

HRESULT SheathWP0070Body::Initialize(void* arg)
{
    if (FAILED(PartObject::Initialize(arg))) return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components SheathWP0070Body");
		return E_FAIL;
	}

	m_RootBoneIndex = m_Model->Get_BoneIndexByName("wp0070");
	if (m_RootBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find WP0070 Root Bone");
		return E_FAIL;
	}

	m_Model->Set_LocalRootNode(m_RootBoneIndex);
    m_Model->Set_Animation(0, 0.f);
    m_Model->Set_AnimLoop(true);

	m_LightSheathMatrix =
		Matrix::CreateRotationX(XMConvertToRadians(90.f)) * Matrix::CreateRotationZ(XMConvertToRadians(-30.f)) *
		Matrix::CreateTranslation(Vector3{ 0.f, 1.5f, -0.4f });

    return S_OK;
}

void SheathWP0070Body::On_Enable()
{
	Pl0000Parts::On_Enable();
	// The sheathed and drawn sword own separate model/cloth instances.
	if (m_SoftBody) m_SoftBody->Set_Simulation(m_SoftBody->Get_Simulation());
}

void SheathWP0070Body::On_Destroy()
{
	Pl0000Parts::On_Destroy();
}

void SheathWP0070Body::Priority_Update(Float timeDelta)
{
}

void SheathWP0070Body::Update(Float timeDelta)
{
}

void SheathWP0070Body::Late_Update(Float timeDelta)
{
    m_Transform->Update_WorldMatrix();
    Update_CombineWorldMatrix(m_LightSheathMatrix * *m_Transform->Get_WorldMatrixPtr());
    m_Model->Update_ModelAnimation(timeDelta);
	m_SoftBody->Simulate(m_Model, m_CombinedWorldMatrix, timeDelta);
}

void SheathWP0070Body::Fixed_Update(Float fixedDelta)
{
}

HRESULT SheathWP0070Body::Render()
{
	if (m_IsActive && m_Visible)
	{
		if (FAILED(Bind_ShaderResources()))
			return E_FAIL;

		size_t numMeshes = m_Model->Get_NumMeshes();
		for (uint32 i = 0; i < numMeshes; ++i)
		{
			if (FAILED(m_Model->Bind_BoneMatrices(m_Shader, BoneMatrices, i))) return E_FAIL;
			if (FAILED(m_Model->BindAndBeginMaterial(m_Shader, i)))
				return E_FAIL;
			if (FAILED(m_Model->Render(i))) return E_FAIL;
		}
	}
	return S_OK;
}

void SheathWP0070Body::Submit_RenderGroup()
{
    if (m_IsActive)
        GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

HRESULT SheathWP0070Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ WeaponShaderTag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"wp0070" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	m_SoftBody = Add_Component<SoftBody>(ETOI(LEVEL::STATIC));
	if (!m_SoftBody) return E_FAIL;

	return S_OK;
}

HRESULT SheathWP0070Body::Bind_ShaderResources()
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

Shared<SheathWP0070Body> SheathWP0070Body::Create(const ComPtr<ID3D11Device>& device,
                                                  const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<SheathWP0070Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : SheathWP0070Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> SheathWP0070Body::Clone(void* arg)
{
	auto instance = make_shared<SheathWP0070Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : SheathWP0070Body");
		return nullptr;
	}

	return instance;
}


