#include "pch.h"
#include "SkyBox.h"
#include <Game.h>

#include "VICube.h"
#include "Shader.h"
#include "Texture.h"

SkyBox::SkyBox() : GameObject{} {}
SkyBox::SkyBox(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{device, context} {}
SkyBox::SkyBox(const SkyBox& rhs)
	: GameObject{rhs} {}

HRESULT SkyBox::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT SkyBox::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
		return E_FAIL;

	if (FAILED(Ready_Components()))
		return E_FAIL;

	// 스카이박스는 Near Plane에 짤리지 않게 스케일을 충분히 키웁니다.
	m_Transform->Set_LocalScale({ 1000.f, 1000.f, 1000.f });

	return S_OK;
}

void SkyBox::Priority_Update(Float timeDelta)
{
}

void SkyBox::Follow_Camera()
{
	/* The cube has a finite size, so it has to stay centred on the viewer or its edges become
	   visible and the corners of the view fall outside it. Only the position follows; the
	   orientation must stay fixed or the sky would turn with the camera.

	   This runs from the render path rather than Priority_Update because the Editor does not
	   step the engine while stopped, yet it still renders every frame. */
	const auto camera = GAME_INSTANCE->Get_MainCamera();
	if (nullptr == camera || nullptr == m_Transform)
		return;

	const auto cameraTransform = camera->Get_Transform();
	if (nullptr == cameraTransform)
		return;

	m_Transform->Set_Position(cameraTransform->Get_Position());
}

void SkyBox::Update(Float timeDelta)
{
	Vector3 camPos{ 
		GAME_INSTANCE->Get_CamTransform().x, 
		GAME_INSTANCE->Get_CamTransform().y, 
		GAME_INSTANCE->Get_CamTransform().z 
	};

	m_Transform->Set_LocalPosition(camPos);
	m_Transform->Update_WorldMatrix();
}

void SkyBox::Late_Update(Float timeDelta)
{
	
}

void SkyBox::Fixed_Update(Float fixedDelta)
{
	
}

HRESULT SkyBox::Render()
{
	if (FAILED(Bind_ShaderResources()))
		return E_FAIL;

	if (FAILED(m_Shader->Begin(0)))
		return E_FAIL;

	if (FAILED(m_CubeBuffer->Bind_Resources()))
		return E_FAIL;

	if (FAILED(m_CubeBuffer->Render()))
		return E_FAIL;

	// Render State 복구 (State Leak으로 인한 다른 오브젝트 꼬임 방지)
	auto context = GAME_INSTANCE->Get_Context();
	context->RSSetState(nullptr);
	context->OMSetDepthStencilState(nullptr, 0);
	context->OMSetBlendState(nullptr, nullptr, 0xffffffff);

	return S_OK;
}

void SkyBox::Submit_RenderGroup()
{
	GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::PRIORITY, shared_from_this());
}

HRESULT SkyBox::Bind_ShaderResources()
{
	Follow_Camera();

	if (FAILED(m_Transform->Bind_ShaderResource(m_Shader, WorldMatrix)))
		return E_FAIL;

	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)))
		return E_FAIL;

	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)))
		return E_FAIL;

	if (FAILED(m_Texture->Bind_ShaderResourceView(m_Shader, "g_Texture", 0)))
		return E_FAIL;

	return S_OK;
}

HRESULT SkyBox::Ready_Components()
{
	Shader::SHADER_DESC shaderDesc{ VTXCUBE::Tag, VTXCUBE::Elements, VTXCUBE::numElements };
	m_Shader = Add_Component<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);

	auto textureDesc = Texture::TEXTURE_DESC{ ETOI(LEVEL::STATIC), L"Skybox_Default" };
	m_Texture = Add_Component<Texture>(ETOI(LEVEL::STATIC), &textureDesc);
	if (nullptr == m_Texture)
		return E_FAIL;

	m_CubeBuffer = Add_Component<VICube>(ETOI(LEVEL::STATIC));

	return S_OK;
}

Shared<SkyBox> SkyBox::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<SkyBox>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : SkyBox");
		return nullptr;
	}

	return prototype;
}

Shared<GameObject> SkyBox::Clone(void* arg)
{
	auto instance = make_shared<SkyBox>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Clone : SkyBox");
		return nullptr;
	}

	return instance;
}


