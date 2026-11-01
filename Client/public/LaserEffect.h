#pragma once
#include <GameObject.h>

#include <random>

NS_BEGIN(Engine)
class Shader;
class Texture;
NS_END

NS_BEGIN(Client)

// Impact of the pod laser (core.eff 229 family): white smoke burst and star flash, dark debris, gold glyphs and
// needles, swirling gold glitter, a rising glitter column and an ember fountain. Everything but the burst sways
// in the wind while it fades. One object plays every impact of the shot (world hit and each pierced monster).
class CLIENT_DLL LaserEffect final : public GameObject
{
public:
	explicit LaserEffect() = default;
	explicit LaserEffect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit LaserEffect(const LaserEffect& rhs);
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

	void Play(const Vector3& position);
	void PlayEvadeBurst(const Vector3& position);

	static Shared<LaserEffect> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;

private:
	enum class KIND : uint8 { GLYPH, GLYPH_STRIP, NEEDLE, RIBBON, RISE, DEBRIS, EMBER };
	struct PARTICLE
	{
		KIND kind{};
		uint32 image{};
		uint32 variant{};		// NEEDLE: glyph image on its tip (0 = none); DEBRIS: piece in debris.dds
		Vector3 position{};		// RIBBON: fixed offset from its point on the ribbon
		Vector3 velocity{};		// NEEDLE: also the direction it points along
		Float delay{};
		Float size{};
		Float length{};
		Float drag{ 1.f };		// velocity kept per 1/60 s
		Float gravity{};
		Float spin{};
		Float phase{};
		Float frequency{};
		Float amplitude{};
		Float ribbonAngle{};
		Float ribbonSpin{};
		Float ribbonT{};
	};
	struct BURST
	{
		Vector3 origin{};
		Float age{};
		Float spin{};
		Bool evade{};
		vector<PARTICLE> particles{};
	};
	struct DRAW
	{
		uint32 image{};
		Bool alphaBlend{};
		uint32 firstVertex{};
		uint32 vertexCount{};
	};

	Float Random(Float minValue, Float maxValue);
	Vector3 Random_Direction();

private:
	Shared<Shader> m_Shader{};
	Shared<Texture> m_Texture{};
	ComPtr<ID3D11Buffer> m_VertexBuffer{};
	vector<BURST> m_Bursts{};
	vector<DRAW> m_Draws{};
	std::mt19937 m_Random{ std::random_device{}() };
};

NS_END
