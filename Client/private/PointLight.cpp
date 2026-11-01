#include "pch.h"
#include "PointLight.h"
#include "Game.h"
#include "Light.h"
#include "Transform.h"
#include "VIBuffer_Particle_Point.h"
#include <Shader.h>
#include <SpdLogger.h>

PointLight::PointLight() : GameObject{} {}
PointLight::PointLight(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
PointLight::PointLight(const PointLight& rhs)
	: GameObject{ rhs }, m_Color{ rhs.m_Color }, m_Range{ rhs.m_Range }, m_Glow{ rhs.m_Glow } {}

HRESULT PointLight::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT PointLight::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize PointLight {}", m_ObjectName);
		return E_FAIL;
	}

	LIGHT_DESC desc{};
	desc.type = LIGHT::POINT;
	m_Light = Light::Create(desc);
	if (nullptr == m_Light || FAILED(GAME_INSTANCE->Add_Light(m_Light)))
	{
		LOG_ERROR(L"Failed to register light for PointLight {}", m_ObjectName);
		return E_FAIL;
	}
	m_Light->Set_Active(Is_Active());
	Submit_RenderGroup();

	return S_OK;
}

void PointLight::On_Destroy()
{
	if (m_Light)
		GAME_INSTANCE->Remove_Light(m_Light);
	m_Light.reset();
	if (m_Buffer)
		m_Buffer->On_Destroy();
	m_Buffer.reset();
	m_Shader.reset();
	GameObject::On_Destroy();
}

void PointLight::On_Enable()
{
	GameObject::On_Enable();
	if (m_Light)
		m_Light->Set_Active(true);
}

void PointLight::On_Disable()
{
	GameObject::On_Disable();
	if (m_Light)
		m_Light->Set_Active(false);
}

/* Runs every frame in both edit and play, so the light follows Transform and Inspector edits. A light with a Glow
   also draws it in the BLEND pass (after the lit composite, under the fog and bloom). */
void PointLight::Submit_RenderGroup()
{
	if (!m_Light)
		return;
	if (m_Glow > 0.f && Is_Active())
	{
		if (!m_Shader)
		{
			Shader::SHADER_DESC shaderDesc{ VTXPARTICLE_POINT_DESC::Tag, VTXPARTICLE_POINT_DESC::Elements, VTXPARTICLE_POINT_DESC::numElements };
			m_Shader = GAME_INSTANCE->Instantiate<Shader>(ETOI(LEVEL::STATIC), &shaderDesc);
			// One unit point at the origin; the glow's quad comes from the world matrix.
			VIBuffer_Particle_Point::VIBUFFER_INSTANCE_POINT_DESC instanceDesc{};
			instanceDesc.numInstances = 1;
			instanceDesc.scale = Vector2{ 1.f, 1.f };
			instanceDesc.lifeTime = Vector2{ 1.f, 1.f };
			m_Buffer = GAME_INSTANCE->Instantiate<VIBuffer_Particle_Point>(ETOI(LEVEL::STATIC), &instanceDesc);
		}
		if (m_Shader && m_Buffer)
			GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::BLEND, shared_from_this());
	}

	LIGHT_DESC desc = m_Light->Get_LightDesc();
	m_Transform->Update_WorldMatrix();
	const Vector3 position = m_Transform->Get_WorldMatrix().Translation();
	desc.position = Vector4{ position.x, position.y, position.z, 1.f };
	desc.diffuse = m_Color;
	desc.specular = m_Color;
	desc.range = (std::max)(m_Range, 0.01f);
	m_Light->Set_LightDesc(desc);
}

/* The bulb: a soft halo in the light's hue and a smaller white-hot core, both facing the camera at the light. The BLEND
   target is already tone mapped, so the hue is the color scaled to its brightest channel. */
HRESULT PointLight::Render()
{
	if (!m_Shader || !m_Buffer)
		return S_OK;
	if (FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ViewMatrix, D3DTS::VIEW)) ||
		FAILED(GAME_INSTANCE->Bind_TransformMatrix(m_Shader, ProjMatrix, D3DTS::PROJ)) ||
		FAILED(GAME_INSTANCE->Bind_CameraPosition(m_Shader, CameraPosition)))
		return E_FAIL;

	const Matrix camera = GAME_INSTANCE->Get_Transform(D3DTS::VIEW).Invert();
	const Vector3 position = m_Transform->Get_WorldMatrix().Translation();
	const Float peak = (std::max)({ m_Color.x, m_Color.y, m_Color.z, 0.0001f });
	const uint32 glow = 1;
	const Float fade = 1.f;
	struct GLOW_LAYER { Float size; Vector4 tint; };
	const GLOW_LAYER layers[] = {
		{ m_Glow, Vector4{ m_Color.x / peak * 0.55f, m_Color.y / peak * 0.55f, m_Color.z / peak * 0.55f, 1.f } },
		{ m_Glow * 0.3f, Vector4{ 1.f, 0.95f, 0.85f, 1.f } },
	};
	for (const GLOW_LAYER& layer : layers)
	{
		Matrix world{ camera.Right() * layer.size, camera.Up() * layer.size, camera.Backward() };
		world.Translation(position);
		if (FAILED(m_Shader->Bind_RawValue("g_QuadGlow", &glow, sizeof(uint32))) ||
			FAILED(m_Shader->Bind_RawValue("g_QuadTint", &layer.tint, sizeof(Vector4))) ||
			FAILED(m_Shader->Bind_RawValue("g_FadeAlpha", &fade, sizeof(Float))) ||
			FAILED(m_Shader->Bind_Matrix(WorldMatrix, &world)) ||
			FAILED(m_Shader->Begin("Quad")) ||
			FAILED(m_Buffer->Bind_Resources()) ||
			FAILED(m_Buffer->Render()))
			return E_FAIL;
	}
	return S_OK;
}

Shared<PointLight> PointLight::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<PointLight>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Created : PointLight");
		MSG_BOX("Failed to Created : PointLight");
	}
	return prototype;
}

Shared<GameObject> PointLight::Clone(void* arg)
{
	auto instance = make_shared<PointLight>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Cloned : PointLight");
		MSG_BOX("Failed to Cloned : PointLight");
	}
	return instance;
}
