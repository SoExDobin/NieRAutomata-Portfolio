#include "pch.h"
#include "DirectionalLight.h"
#include "Game.h"
#include "Light.h"
#include "Transform.h"
#include <SpdLogger.h>

DirectionalLight::DirectionalLight() : GameObject{} {}
DirectionalLight::DirectionalLight(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: GameObject{ device, context } {}
DirectionalLight::DirectionalLight(const DirectionalLight& rhs)
	: GameObject{ rhs }, m_Color{ rhs.m_Color }, m_Ambient{ rhs.m_Ambient }, m_Exposure{ rhs.m_Exposure },
	m_Contrast{ rhs.m_Contrast }, m_Saturation{ rhs.m_Saturation }, m_Tint{ rhs.m_Tint }, m_FogColor{ rhs.m_FogColor },
	m_FogDistance{ rhs.m_FogDistance }, m_FogOpacity{ rhs.m_FogOpacity }, m_FogSunColor{ rhs.m_FogSunColor },
	m_Bloom{ rhs.m_Bloom } {}

HRESULT DirectionalLight::Initialize_Prototype()
{
	return GameObject::Initialize_Prototype();
}

HRESULT DirectionalLight::Initialize(void* arg)
{
	if (FAILED(GameObject::Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Initialize DirectionalLight {}", m_ObjectName);
		return E_FAIL;
	}

	/* Aim down and across by default; a loaded scene overwrites this with the saved rotation. */
	m_Transform->Set_Rotation(Vector3{ 50.f, -30.f, 0.f });

	LIGHT_DESC desc{};
	desc.type = LIGHT::DIRECTIONAL;
	m_Light = Light::Create(desc);
	if (nullptr == m_Light || FAILED(GAME_INSTANCE->Add_Light(m_Light)))
	{
		LOG_ERROR(L"Failed to register light for DirectionalLight {}", m_ObjectName);
		return E_FAIL;
	}
	m_Light->Set_Active(Is_Active());
	Submit_RenderGroup();

	return S_OK;
}

void DirectionalLight::On_Destroy()
{
	if (m_Light)
		GAME_INSTANCE->Remove_Light(m_Light);
	m_Light.reset();
	GameObject::On_Destroy();
}

void DirectionalLight::On_Enable()
{
	GameObject::On_Enable();
	if (m_Light)
		m_Light->Set_Active(true);
}

void DirectionalLight::On_Disable()
{
	GameObject::On_Disable();
	if (m_Light)
		m_Light->Set_Active(false);
}

void DirectionalLight::Set_AreaLight(const Color& color, const Color& ambient, Float exposure, Float fogOpacity)
{
	m_AreaColor = color;
	m_AreaAmbient = ambient;
	m_AreaExposure = exposure;
	m_AreaFogOpacity = fogOpacity;
}

/* Play only: eases into or out of the area light over half a second, so walking in is not a cut. */
void DirectionalLight::Update(Float timeDelta)
{
	constexpr Float blendSeconds = 0.5f;
	const Float step = timeDelta / blendSeconds;
	m_AreaWeight = m_AreaCount > 0 ? (std::min)(m_AreaWeight + step, 1.f) : (std::max)(m_AreaWeight - step, 0.f);
}

/* Draws nothing; runs every frame in both edit and play, so the light follows Transform and
   Inspector edits without depending on the gameplay update loop. */
void DirectionalLight::Submit_RenderGroup()
{
	if (!m_Light)
		return;

	LIGHT_DESC desc = m_Light->Get_LightDesc();
	m_Transform->Update_WorldMatrix();
	Vector3 look = m_Transform->Get_Look();
	look.Normalize();
	desc.direction = Vector4{ look.x, look.y, look.z, 0.f };
	const Color color = Color::Lerp(m_Color, m_AreaColor, m_AreaWeight);
	desc.diffuse = color;
	desc.ambient = Color::Lerp(m_Ambient, m_AreaAmbient, m_AreaWeight);
	desc.specular = color;
	m_Light->Set_LightDesc(desc);

	if (Is_Active())
	{
		const Float exposure = m_Exposure + (m_AreaExposure - m_Exposure) * m_AreaWeight;
		GAME_INSTANCE->Set_ColorGrade(Vector3{ exposure, m_Contrast, m_Saturation }, m_Tint);
		GAME_INSTANCE->Set_Fog(m_FogColor, m_FogDistance, m_FogOpacity + (m_AreaFogOpacity - m_FogOpacity) * m_AreaWeight,
			m_FogSunColor);
		GAME_INSTANCE->Set_Bloom(m_Bloom);
	}
}

Shared<DirectionalLight> DirectionalLight::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto prototype = make_shared<DirectionalLight>(device, context);

	if (FAILED(prototype->Initialize_Prototype()))
	{
		LOG_ERROR(L"Failed to Created : DirectionalLight");
		MSG_BOX("Failed to Created : DirectionalLight");
	}
	return prototype;
}

Shared<GameObject> DirectionalLight::Clone(void* arg)
{
	auto instance = make_shared<DirectionalLight>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		LOG_ERROR(L"Failed to Cloned : DirectionalLight");
		MSG_BOX("Failed to Cloned : DirectionalLight");
	}
	return instance;
}
