#include "pch.h"
#include "SkySphere.h"
#include <Game.h>
#include "VISphere.h"
#include "Shader.h"
#include "Texture.h"

SkySphere::SkySphere() : GameObject{} {}
SkySphere::SkySphere(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {
}
SkySphere::SkySphere(const SkySphere& rhs)
	: GameObject{ rhs } {
}
HRESULT SkySphere::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}
HRESULT SkySphere::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg))) 
		return E_FAIL;

	if (FAILED(Ready_Components())) 
		return E_FAIL;

	// 스카이박스는 매우 크게 설정
	m_Transform->Set_LocalScale({ 1.f, 1.f, 1.f });

	return S_OK;
}

void SkySphere::Priority_Update(Float timeDelta)
{
	GameObject::Priority_Update(timeDelta);
	Vector3 camPos{ GAME_INSTANCE->Get_CamTransform() };
	m_Transform->Set_Position(camPos);
	m_Transform->Update_WorldMatrix();
}

void SkySphere::Update(Float timeDelta)
{

}

void SkySphere::Late_Update(Float timeDelta)
{
	
}

void SkySphere::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT SkySphere::Render()
{
	if (FAILED(Bind_ShaderResources())) 
		return E_FAIL;
	if (FAILED(m_Shader->Begin(0))) 
		return E_FAIL;
	if (FAILED(m_SphereBuffer->Bind_Resources())) 
		return E_FAIL;
	if (FAILED(m_SphereBuffer->Render())) 
		return E_FAIL;

	auto context = GAME_INSTANCE->Get_Context();
	context->RSSetState(nullptr);                               // Rasterizer State 기본값 복구 (Cull Back)
	context->OMSetDepthStencilState(nullptr, 0);                // Depth Stencil State 기본값 복구 (Less_Equal)
	context->OMSetBlendState(nullptr, nullptr, 0xffffffff);     // Blend State 기본값 복구

	return S_OK;
}

void SkySphere::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::PRIORITY, shared_from_this());
}

HRESULT SkySphere::Bind_ShaderResources()
{
	if (FAILED(m_Transform->Bind_ShaderResource(m_Shader, WorldMatrix)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;
	if (m_Texture->Bind_ShaderResourceView(m_Shader, "g_Texture", 0)) // CubeMap 샘플링
		return E_FAIL;

	return S_OK;
}

HRESULT SkySphere::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXCUBE::Tag, VTXCUBE::Elements, VTXCUBE::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
	auto textureDesc = Texture::TEXTURE_DESC{ ETOI(LEVEL::STATIC), L"Skybox_Default" };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::STATIC), &textureDesc);
	m_SphereBuffer = Add_Component<VISphere>(ETOI(LEVEL::STATIC));
	return S_OK;
}

Shared<SkySphere> SkySphere::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<SkySphere>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : SkySphere");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> SkySphere::Clone(void* arg)
{
	auto instance = make_shared<SkySphere>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : SkySphere");
		return nullptr;
	}

	return instance;
}


