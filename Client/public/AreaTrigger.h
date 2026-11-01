#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
class OBBCollider;
class Shader;
class Texture;
class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

class DirectionalLight;

/* Scene-placed box (its OBBCollider's Offset/Extents) that reacts to the player:
   - LightOverride: the level's DirectionalLight blends to LightColor/LightAmbient/LightExposure/LightFogOpacity while the
     player is inside (an interior turns the level fog off with LightFogOpacity 0). The area is indoors, so the sky's
     Firework loops are silenced meanwhile (em3000's dome).
   - NextLevel: entering loads that level through Loading (-1 = none); the fade waits until no communication plays.
     The way out is marked on the floor under the box: coreeff 102/11 (a square frame) glows pale gold in the EMISSIVE
     group, its line stretched along the box's footprint, so it reads in the dark.
   - Communication: entering queues that COMMUNICATION_CUE once per level visit (-1 = none, 0 = CityBriefing,
     4 = CityHandover, the call before the Level 4 fade; 5 = ParkBriefing, 6 = ParkEntrance).
   - SoundEmitter component: with a SoundTag it loops that ambience at the trigger's position. */
class CLIENT_DLL AreaTrigger final : public GameObject
{
public:
	explicit AreaTrigger();
	explicit AreaTrigger(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit AreaTrigger(const AreaTrigger& rhs);
	virtual ~AreaTrigger() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Priority_Update(Float timeDelta) override {}
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override {}
	void Fixed_Update(Float fixedDelta) override {}
	void Submit_RenderGroup() override;
	HRESULT Render() override;

private:
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

public:
	Bool Get_LightOverride() const { return m_LightOverride; }
	void Set_LightOverride(Bool lightOverride) { m_LightOverride = lightOverride; }
	const Color& Get_LightColor() const { return m_LightColor; }
	void Set_LightColor(const Color& color) { m_LightColor = color; }
	const Color& Get_LightAmbient() const { return m_LightAmbient; }
	void Set_LightAmbient(const Color& ambient) { m_LightAmbient = ambient; }
	Float Get_LightExposure() const { return m_LightExposure; }
	void Set_LightExposure(Float exposure) { m_LightExposure = exposure; }
	Float Get_LightFogOpacity() const { return m_LightFogOpacity; }
	void Set_LightFogOpacity(Float opacity) { m_LightFogOpacity = opacity; }
	int32 Get_NextLevel() const { return m_NextLevel; }
	void Set_NextLevel(int32 nextLevel) { m_NextLevel = nextLevel; }
	int32 Get_CameraEvent() const { return m_CameraEvent; }
	void Set_CameraEvent(int32 event) { m_CameraEvent = event; }
	int32 Get_Communication() const { return m_Communication; }
	void Set_Communication(int32 communication) { m_Communication = communication; }

private:
	Bool Is_Player(const Shared<Collider>& targetCollider) const;
	// Inside: the level's lights get this area's values every frame (Inspector edits apply at once).
	void Enter_AreaLight();
	void Leave_AreaLight();
	void Request_NextLevel();

private:
	Shared<OBBCollider> m_Box{ nullptr };
	Shared<Shader> m_Shader{ nullptr };
	Shared<Texture> m_PortalTexture{ nullptr };
	Shared<VIBuffer_Particle_Point> m_PortalBuffer{ nullptr };
	vector<Weak<DirectionalLight>> m_AreaLights{};
	Bool m_LightOverride{ false };
	Color m_LightColor{ 0.f, 0.f, 0.f, 1.f };
	Color m_LightAmbient{ 0.08f, 0.08f, 0.09f, 1.f };
	Float m_LightExposure{ 1.f };
	Float m_LightFogOpacity{ 0.f };
	int32 m_NextLevel{ -1 };
	Bool m_LevelPending{ false };
	Bool m_LevelRequested{ false };
	int32 m_Communication{ -1 };
	int32 m_CameraEvent{ -1 };

	static inline const Vector4 PortalTint{ 1.8f, 1.5f, 1.f, 1.f };	// pale ivory gold, paler than the pod laser's; the bloom spreads it softly
	static inline const Vector2 PortalFrameSpan{ 0.63f, 0.69f };		// the frame line's span in the texture, u and v
	static constexpr Float PortalFloorLift = 0.05f;	// m over the floor, clear of its depth (the pass tests, never writes)
	static constexpr Float PortalFloorProbe = 1.f;	// m below the box the floor is still looked for

public:
	static Shared<AreaTrigger> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
