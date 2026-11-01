#include "pch.h"
#include "DamageText.h"
#include "ThirdPersonCamera.h"

#include <Game.h>
#include <WorldUIObject.h>

namespace
{
	// The reference's numbers: about 16 px tall on the 1920x1080 canvas, widely spaced, pale.
	constexpr Float DamageScale = 0.5f;			// Nier_32
	constexpr Float DamageSpacing = 4.f;
	constexpr Float DamagePopTime = 0.08f;		// starts this much bigger and settles
	constexpr Float DamagePopScale = 1.35f;
	constexpr Float DamageLifetime = 0.9f;
	constexpr Float DamageFadeStart = 0.55f;
	constexpr Float DamageRise = 24.f;			// pixels over its life
	constexpr Float DamageScatter = 12.f;
	const Color DamageColor{ 0.93f, 0.91f, 0.86f, 1.f };
}

DamageText::DamageText(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
	: TextUI(device, context) {}
DamageText::DamageText(const DamageText& rhs) : TextUI(rhs) {}

void DamageText::Show(const Vector3& worldPosition, Float damage)
{
	const wstring text = to_wstring(std::max(1, static_cast<int32>(lroundf(damage))));
	DAMAGE_TEXT_DESC desc{};
	desc.anchor = UI_ANCHOR::TOP_LEFT;
	desc.fontTag = L"Nier_32";
	desc.text = text.c_str();
	desc.color = DamageColor;
	desc.fontScale = DamageScale;
	desc.spacing = DamageSpacing;
	desc.pivot = 0.5f;
	desc.worldPosition = worldPosition;
	GAME_INSTANCE->Instantiate<DamageText>(L"DamageText", GAME_INSTANCE->Get_CurrentLevelIndex(), &desc);
}

HRESULT DamageText::Initialize(void* arg)
{
	if (!arg || FAILED(TextUI::Initialize(arg)))
		return E_FAIL;
	m_WorldPosition = static_cast<DAMAGE_TEXT_DESC*>(arg)->worldPosition;
	static std::mt19937 random{ std::random_device{}() };
	std::uniform_real_distribution<Float> scatter{ -DamageScatter, DamageScatter };
	m_Scatter = Vector2{ scatter(random), scatter(random) };
	m_Color.w = 0.f;	// shown from its first Late_Update, once it has a place on screen
	return S_OK;
}

void DamageText::Late_Update(Float timeDelta)
{
	TextUI::Late_Update(timeDelta);
	m_Time += timeDelta;
	if (m_Time >= DamageLifetime)
	{
		GAME_INSTANCE->Destroy(Get_ObjectGuid());
		return;
	}

	// Hidden while a cinematic holds the camera; its lifetime keeps running.
	const auto camera = Cast<ThirdPersonCamera>(GAME_INSTANCE->Get_MainCamera());
	Vector2 position{};
	if ((camera && camera->Is_Held()) || !WorldUIObject::To_Canvas(m_WorldPosition, position))
	{
		m_Color.w = 0.f;
		return;
	}
	const Float life = m_Time / DamageLifetime;
	const Float rise = DamageRise * (1.f - (1.f - life) * (1.f - life));
	m_Transform->Set_LocalPosition(position.x + m_Scatter.x, position.y + m_Scatter.y - rise, 0.f);

	const Float pop = std::clamp(m_Time / DamagePopTime, 0.f, 1.f);
	m_FontScale = DamageScale * (DamagePopScale + (1.f - DamagePopScale) * pop);
	m_Color.w = 1.f - std::clamp((m_Time - DamageFadeStart) / (DamageLifetime - DamageFadeStart), 0.f, 1.f);
}

Shared<DamageText> DamageText::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context)
{
	auto result = make_shared<DamageText>(device, context);
	return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}

Shared<GameObject> DamageText::Clone(void* arg)
{
	auto result = make_shared<DamageText>(*this);
	return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
