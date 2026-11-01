#pragma once
#include "ParticleEffect.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL FireFlashEffect final : public ParticleEffect
{
public:
	explicit FireFlashEffect();
	explicit FireFlashEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit FireFlashEffect(const FireFlashEffect& rhs);
	~FireFlashEffect() override = default;
	
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override { ParticleEffect::On_Destroy(); }
	void On_Enable() override { ParticleEffect::On_Enable(); }
	void On_Disable() override { ParticleEffect::On_Disable(); }

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	void Submit_RenderGroup() override;
	HRESULT Render() override;

public:
	// One Pl0000 factory child draws the Pod's muzzle: the sigil (core.eff/028, 041/8) while the Pod shoots or fires the laser,
	// and a flash (core.eff/034, 034/0) relit by every shot. WP3000Body keeps it on the muzzle every frame.
	void Flash();
	void Set_Muzzle(const Vector3& position, const Quaternion& facing, Bool sigilShown);

private:
	HRESULT Ready_Components();
	HRESULT Bind_ShaderResources();
	HRESULT Render_Quad(uint32 frame, const Matrix& world, const Vector4& tint, Float fade);

private:
	// Shorter than the Pod's fire interval (0.075), so every shot blinks.
	static constexpr Float FlashTime = 0.05f;
	static constexpr Float SigilFadeTime = 0.2f;

	Shared<Shader> m_Shader{ nullptr };
	Shared<Texture> m_Texture{ nullptr };
	Shared<VIBuffer_Particle_Point>  m_Buffer{ nullptr };
	Float m_FlashRemaining{ 0.f };
	Float m_FlashSize{ 1.f };
	Float m_FlashRoll{ 0.f };
	Float m_SigilAlpha{ 0.f };
	Bool m_SigilShown{ false };
	Vector3 m_MuzzlePosition{};
	Quaternion m_MuzzleFacing{};

public:
	static Shared<FireFlashEffect> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
