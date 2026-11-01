#pragma once
#include "ParticleEffect.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

// One explosion billboard that plays a whole Texture/Effect/Explode sequence (Explode_1~3, picked at random)
// once and fades over its smoke tail. Spawned by a dying machine (StateEm0010_Dead) and outlives it.
// A desc texture tag plays that sequence instead: 2B's shockwaves raise the coreeff 017 ground dust (Pl0000Shockwave).
class CLIENT_DLL ExplodeEffect final : public ParticleEffect
{
public:
	typedef struct tagExplodeEffectDesc : public PARTICLE_EFFECT_DESC
	{
		Vector3 position{};
		Float size{ 4.f };
		wstring textureTag{};				// empty: a random Effect_Explode_1~3
		Vector4 tint{ 1.f, 1.f, 1.f, 1.f };	// rgb multiplies the frame colour, a its coverage
		Bool upright{ false };				// stands straight up instead of rolling with the camera
		Float framesPerSecond{ 60.f };
		Float fadeStart{ 0.7f };			// fraction of the sequence after which it fades out
	} EXPLODE_EFFECT_DESC;

public:
	explicit ExplodeEffect();
	explicit ExplodeEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit ExplodeEffect(const ExplodeEffect& rhs);
	~ExplodeEffect() override = default;

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
	HRESULT Ready_Components(const EXPLODE_EFFECT_DESC& desc);
	HRESULT Bind_ShaderResources();

private:
	Shared<Shader> m_Shader{ nullptr };
	Shared<Texture> m_Texture{ nullptr };
	Shared<VIBuffer_Particle_Point> m_Buffer{ nullptr };
	Float m_Time{};
	uint32 m_FrameCount{};
	Vector4 m_Tint{ 1.f, 1.f, 1.f, 1.f };
	Bool m_Upright{ false };
	Float m_FramesPerSecond{ 60.f };
	Float m_FadeStart{ 0.7f };

public:
	static Shared<ExplodeEffect> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
