#pragma once
#include "ParticleEffect.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

// A fistful of paper confetti a park machine throws (em0000 e060: em000f.eff 200 scatters data010 texture 125 from its
// hands). One draw places every piece from its instance index (Shader_VtxParticlePoint Confetti pass): thrown out round
// the throw, the air takes their speed and they flutter down swaying, settle on the floor and fade; each piece shows one
// of Effect_Confetti's four papers (columns) tumbling through its 30 frames (rows).
class CLIENT_DLL ConfettiEffect final : public ParticleEffect
{
public:
	typedef struct tagConfettiEffectDesc : public PARTICLE_EFFECT_DESC
	{
		Vector3 position{};
		Vector3 throwDirection{ 0.f, 1.f, 0.f };
		Float floorHeight{ 0.f };		// world y the pieces come to rest on
		uint32 count{ 24 };
	} CONFETTI_EFFECT_DESC;

public:
	explicit ConfettiEffect();
	explicit ConfettiEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit ConfettiEffect(const ConfettiEffect& rhs);
	~ConfettiEffect() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override { ParticleEffect::On_Destroy(); }
	void On_Enable() override { ParticleEffect::On_Enable(); }
	void On_Disable() override { ParticleEffect::On_Disable(); }

public:
	void Update(Float timeDelta) override;
	void Submit_RenderGroup() override;
	HRESULT Render() override;

private:
	HRESULT Ready_Components(const CONFETTI_EFFECT_DESC& desc);

private:
	static constexpr uint32 MaxPieces = 48;
	static constexpr Float Life = 4.f;	// Shader_VtxParticlePoint CONFETTI_LIFE

	Shared<Shader> m_Shader{ nullptr };
	Shared<Texture> m_Texture{ nullptr };
	Shared<VIBuffer_Particle_Point> m_Buffer{ nullptr };
	Vector4 m_Burst{};		// xyz origin, w age (g_ConfettiBurst)
	Vector4 m_Aim{};		// xyz throw, w seed (g_ConfettiAim)
	Float m_FloorHeight{};
	uint32 m_Count{};

public:
	static Shared<ConfettiEffect> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
