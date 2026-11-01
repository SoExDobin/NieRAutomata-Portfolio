#include "pch.h"
#include "SheathWP0220Body.h"

#include <SpdLogger.h>

#include "Model.h"
#include "Shader.h"
#include "Game.h"
#include "SheathWP0070Body.h"


SheathWP0220Body::SheathWP0220Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: Pl0000Parts{ device, context } { }
SheathWP0220Body::SheathWP0220Body(const SheathWP0220Body& rhs)
	: Pl0000Parts{ rhs } {}

HRESULT SheathWP0220Body::Initialize_Prototype()
{
	return Pl0000Parts::Initialize_Prototype();
}

HRESULT SheathWP0220Body::Initialize(void* arg)
{
	if (FAILED(PartObject::Initialize(arg))) return E_FAIL;

	if (FAILED(Ready_Components()))
	{
		LOG_ERROR(L"Failed to Ready_Components SheathWP0220Body");
		return E_FAIL;
	}

	m_RootBoneIndex = m_Model->Get_BoneIndexByName("wp0220");
	if (m_RootBoneIndex == -1)
	{
		LOG_ERROR(L"Failed to Find WP0220 Root Bone");
		return E_FAIL;
	}

	m_Model->Set_LocalRootNode(m_RootBoneIndex);
	m_Model->Set_Animation(0, 0.f);
	m_Model->Set_AnimLoop(true);

	m_HeavySheathMatrix =
		Matrix::CreateRotationX(XMConvertToRadians(90.f)) * 
		Matrix::CreateRotationY(XMConvertToRadians(-40.f)) * 
		Matrix::CreateRotationZ(XMConvertToRadians(-25.f)) *
		Matrix::CreateTranslation(Vector3{ 0.f, 1.5f, -0.65f });

	return S_OK;
}

void SheathWP0220Body::On_Destroy()
{
	Pl0000Parts::On_Destroy();
}

void SheathWP0220Body::Priority_Update(Float timeDelta)
{
}

void SheathWP0220Body::Update(Float timeDelta)
{
}

void SheathWP0220Body::Late_Update(Float timeDelta)
{
	m_Transform->Update_WorldMatrix();
	Update_CombineWorldMatrix(m_HeavySheathMatrix * *m_Transform->Get_WorldMatrixPtr());
	m_Model->Update_ModelAnimation(timeDelta);
}

void SheathWP0220Body::Fixed_Update(Float fixedDelta)
{
}

HRESULT SheathWP0220Body::Render()
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

void SheathWP0220Body::Submit_RenderGroup()
{
	if (m_IsActive)
		GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::NONBLEND, shared_from_this());
}

HRESULT SheathWP0220Body::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ WeaponShaderTag,  VTXANIMMESH::Elements, VTXANIMMESH::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	if (nullptr == m_Shader)
		return E_FAIL;

	Model::MODEL_DESC modelDesc{ L"wp0220" };
	m_Model = Add_Component<Model>(ETOI(LEVEL::STATIC), &modelDesc);
	if (nullptr == m_Model)
		return E_FAIL;
	if (FAILED(m_Model->Validate_MaterialBindings(m_Shader))) return E_FAIL;

	return S_OK;
}

HRESULT SheathWP0220Body::Bind_ShaderResources()
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

Shared<SheathWP0220Body> SheathWP0220Body::Create(const ComPtr<ID3D11Device>& device,
	const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<SheathWP0220Body>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : SheathWP0220Body");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> SheathWP0220Body::Clone(void* arg)
{
	auto instance = make_shared<SheathWP0220Body>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : SheathWP0220Body");
		return nullptr;
	}

	return instance;
}


