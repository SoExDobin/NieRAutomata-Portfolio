#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
	class Light;
	class Shader;
	class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

/* Scene-placed point light (lamp, neon): lights the G-buffer around the Transform's position out to Range.
   Color above 1 is its brightness. No shadow. Glow above 0 also draws the lamp's bulb at the light: a camera-facing
   soft halo Glow meters across in the light's hue with a white-hot core (the original lamp heads are lit by such
   sprites, their meshes are not emissive). Color, Range and Glow are reflected for the Inspector and the scene. */
class CLIENT_DLL PointLight final : public GameObject
{
public:
	explicit PointLight();
	explicit PointLight(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit PointLight(const PointLight& rhs);
	virtual ~PointLight() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;
	void On_Enable() override;
	void On_Disable() override;

public:
	void Priority_Update(Float timeDelta) override {}
	void Update(Float timeDelta) override {}
	void Late_Update(Float timeDelta) override {}
	void Fixed_Update(Float fixedDelta) override {}
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	const Color& Get_Color() const { return m_Color; }
	void Set_Color(const Color& color) { m_Color = color; }
	Float Get_Range() const { return m_Range; }
	void Set_Range(Float range) { m_Range = range; }
	Float Get_Glow() const { return m_Glow; }
	void Set_Glow(Float glow) { m_Glow = glow; }

private:
	Shared<Light> m_Light{ nullptr };
	Color m_Color{ 4.f, 3.2f, 2.2f, 1.f };
	Float m_Range{ 8.f };
	Float m_Glow{ 0.f };
	// Made on the first frame with a Glow and not attached as components, so the many lights without one carry no
	// draw resources and the scene document keeps only the Transform.
	Shared<Shader> m_Shader{ nullptr };
	Shared<VIBuffer_Particle_Point> m_Buffer{ nullptr };

public:
	static Shared<PointLight> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
