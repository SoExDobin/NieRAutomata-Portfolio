#pragma once
#include "TextUI.h"

NS_BEGIN(Client)

/* A hit's damage as a small spaced number where it landed (the reference's "6", "44" over the boss), for the player's
   hits on monsters and the monsters' on the player alike. It pops in, drifts up following that world point and fades;
   made at runtime by Entity::TakeDamage for each hit that takes HP, gone when it fades. */
class CLIENT_DLL DamageText final : public TextUI
{
public:
	typedef struct tagDamageTextDesc : public UI_TEXT_DESC
	{
		Vector3 worldPosition{};
	} DAMAGE_TEXT_DESC;

public:
	// Made in the current level: the hit may land while a level's objects are being walked.
	static void Show(const Vector3& worldPosition, Float damage);

public:
	DamageText() = default;
	DamageText(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	DamageText(const DamageText& rhs);
	HRESULT Initialize(void* arg) override;
	void Late_Update(Float timeDelta) override;

private:
	Vector3 m_WorldPosition{};
	Vector2 m_Scatter{};		// keeps numbers of one burst apart
	Float m_Time{ 0.f };

public:
	static Shared<DamageText> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg = nullptr) override;
};

NS_END
