#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
	class Light;
NS_END

NS_BEGIN(Client)

/* Scene-placed directional light. The light shines along the Transform's look axis, so it is
   aimed by rotating the object; Color and Ambient are reflected for the Inspector and the scene.
   It also owns the level's screen grading (Exposure, Contrast, Saturation, Tint) and fog (FogColor, FogDistance,
   FogOpacity, FogSunColor added toward the sun; distance 0 = no fog) and Bloom (0 = off): one light per level sets the level's look, and the renderer falls back to
   neutral where a level has none. */
class CLIENT_DLL DirectionalLight final : public GameObject
{
public:
	explicit DirectionalLight();
	explicit DirectionalLight(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit DirectionalLight(const DirectionalLight& rhs);
	virtual ~DirectionalLight() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void On_Enable() override;
	void On_Disable() override;

public:
	void Priority_Update(Float timeDelta) override {}
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override {}
	void Fixed_Update(Float fixedDelta) override {}
	HRESULT Render() override { return S_OK; }
	void Submit_RenderGroup() override;

public:
	const Color& Get_Color() const { return m_Color; }
	void Set_Color(const Color& color) { m_Color = color; }
	const Color& Get_Ambient() const { return m_Ambient; }
	void Set_Ambient(const Color& ambient) { m_Ambient = ambient; }
	Float Get_Exposure() const { return m_Exposure; }
	void Set_Exposure(Float exposure) { m_Exposure = exposure; }
	Float Get_Contrast() const { return m_Contrast; }
	void Set_Contrast(Float contrast) { m_Contrast = contrast; }
	Float Get_Saturation() const { return m_Saturation; }
	void Set_Saturation(Float saturation) { m_Saturation = saturation; }
	const Color& Get_Tint() const { return m_Tint; }
	void Set_Tint(const Color& tint) { m_Tint = tint; }
	const Color& Get_FogColor() const { return m_FogColor; }
	void Set_FogColor(const Color& color) { m_FogColor = color; }
	Float Get_FogDistance() const { return m_FogDistance; }
	void Set_FogDistance(Float distance) { m_FogDistance = distance; }
	Float Get_FogOpacity() const { return m_FogOpacity; }
	void Set_FogOpacity(Float opacity) { m_FogOpacity = opacity; }
	const Color& Get_FogSunColor() const { return m_FogSunColor; }
	void Set_FogSunColor(const Color& color) { m_FogSunColor = color; }
	Float Get_Bloom() const { return m_Bloom; }
	void Set_Bloom(Float bloom) { m_Bloom = bloom; }

	/* An area such as the sewer swaps in its own light while the player is inside (AreaTrigger).
	   The authored values stay as saved; the light blends toward the area values in play. The area
	   exposure darkens the whole screen, sky included, which the light alone cannot reach; the area fog opacity
	   replaces the level's (0 inside a building). Touching areas (the manhole shaft over the sewer) count, so leaving
	   one while still inside the other keeps the area light. The values are set again every frame while inside, so
	   Inspector edits show at once. */
	void Enter_AreaLight() { ++m_AreaCount; }
	void Set_AreaLight(const Color& color, const Color& ambient, Float exposure, Float fogOpacity);
	void Clear_AreaLight() { m_AreaCount = m_AreaCount > 0 ? m_AreaCount - 1 : 0; }

private:
	Shared<Light> m_Light{ nullptr };
	Color m_Color{ 1.f, 1.f, 1.f, 1.f };
	Color m_Ambient{ 0.2f, 0.2f, 0.2f, 1.f };
	Float m_Exposure{ 1.f };
	Float m_Contrast{ 1.f };
	Float m_Saturation{ 1.f };
	Color m_Tint{ 1.f, 1.f, 1.f, 1.f };
	Color m_FogColor{ 1.f, 1.f, 1.f, 1.f };
	Float m_FogDistance{ 0.f };
	Float m_FogOpacity{ 0.f };
	Color m_FogSunColor{ 0.f, 0.f, 0.f, 1.f };
	Float m_Bloom{ 0.f };
	Color m_AreaColor{};
	Color m_AreaAmbient{};
	Float m_AreaExposure{ 1.f };
	Float m_AreaFogOpacity{ 0.f };
	uint32 m_AreaCount{ 0 };
	Float m_AreaWeight{ 0.f };

public:
	static Shared<DirectionalLight> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
