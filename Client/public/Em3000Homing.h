#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
class Shader;
class Texture;
NS_END

NS_BEGIN(Client)

// Em3000 danmak 2 homing laser (em3000_homing_laser_shot / flyloop / explo): a white light with a long trail that
// leaves the chest, rises, then dives fast at the player with a limited turn rate. Once it dives it never climbs again,
// so a shot too fast to turn onto the player runs into the floor. It bursts on contact or on the floor, hurting anyone
// close to the burst.
class CLIENT_DLL Em3000Homing final : public GameObject
{
public:
	typedef struct tagEm3000HomingDesc : public GAMEOBJECT_DESC
	{
		Weak<GameObject> target{};
		Vector3 position{};
		Vector3 direction{};	// launch direction; it turns to the target after the rise
		Float groundY{};
	} EM3000_HOMING_DESC;

public:
	explicit Em3000Homing() = default;
	explicit Em3000Homing(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em3000Homing(const Em3000Homing& rhs);
	~Em3000Homing() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

private:
	void Explode(const Vector3& position);

private:
	Shared<Shader> m_Shader{};
	Shared<Texture> m_Texture{};
	ComPtr<ID3D11Buffer> m_VertexBuffer{};
	Weak<GameObject> m_Target{};
	Vector3 m_Position{};
	Vector3 m_Velocity{};
	Float m_GroundY{};
	Float m_Age{};
	Float m_TrailTimer{};
	deque<Vector3> m_Trail{};	// newest first
	uint32 m_TrailVertices{};
	Bool m_Exploded{ false };

	// Flight: it rises along its launch direction, then steers to the target's chest while heading down at least a slope
	// that grows the longer it homes (SinkGrowth, up to MaxSink), until the floor or the target stops it.
	static constexpr Float RiseTime = 0.55f;
	static constexpr Float RiseSpeed = 12.f;
	static constexpr Float HomingSpeed = 15.f;
	static constexpr Float TurnRate = 2.6f;		// rad/s
	static constexpr Float SinkGrowth = 0.12f;	// the least downward slope (sine) grows this much a second of homing
	static constexpr Float MaxSink = 0.9f;
	static constexpr Float Lifetime = 10.f;		// a backstop: the sink brings every shot down to the floor well before
	static constexpr Float ChestHeight = 1.f;
	static constexpr Float ContactRadius = 0.9f;

	static constexpr Float BlastRadius = 2.f;
	static constexpr Float BlastSize = 3.5f;
	static constexpr Float Damage = 15.f;
	// Burst sound: half the boss's own SoundEmitter level (a volley of them drowned the rest), at its distances.
	static constexpr Float BlastVolume = 0.15f;
	static constexpr Float BlastNear = 10.f;
	static constexpr Float BlastFar = 80.f;

	// White trail (000/0's soft cross-section) behind a white glow (000/1) and a star flare (000/5). HDR, additive.
	static constexpr uint32 TrailImage = 1;
	static constexpr uint32 HeadImage = 2;
	static constexpr uint32 FlareImage = 3;
	static constexpr uint32 TrailPoints = 24;
	static constexpr Float TrailStep = 0.025f;		// seconds between trail points
	static constexpr Float TrailWidth = 0.35f;
	static constexpr Float HeadSize = 0.9f;
	static constexpr Float FlareSize = 1.8f;
	static inline const Vector3 TrailTint{ 1.6f, 1.6f, 1.75f };
	static inline const Vector3 HeadTint{ 2.5f, 2.5f, 2.6f };
	static inline const Vector3 FlareTint{ 1.4f, 1.3f, 1.2f };

	static constexpr uint32 TrailVertexCapacity = (TrailPoints - 1) * 6;
	static constexpr uint32 HeadFirst = TrailVertexCapacity;
	static constexpr uint32 FlareFirst = HeadFirst + 6;
	static constexpr uint32 VertexCapacity = FlareFirst + 6;

public:
	static Shared<Em3000Homing> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
