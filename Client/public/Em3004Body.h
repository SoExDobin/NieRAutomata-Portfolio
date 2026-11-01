#pragma once
#include "PartObject.h"

NS_BEGIN(Engine)
	class AABBCollider;
	class Model;
	class SoundEmitter;
NS_END

NS_BEGIN(Client)

class CLIENT_DLL Em3004Body final : public PartObject
{
public:
	typedef struct tagEm3004BodyDesc : public PARTOBJECT_DESC
	{
	} EM3004BODY_DESC;

public:
	explicit Em3004Body();
	explicit Em3004Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em3004Body(const Em3004Body& rhs);
	~Em3004Body() override = default;

public:
	void Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop);
	uint32 Get_CurrentAnimationIndex() const;
	Bool Is_AnimationFinished() const;
	// Clip playback speed (1: as authored); 0 holds the pose, as the drop from the ceiling holds the fall's first frame.
	void Set_AnimationRate(Float rate) { m_AnimationRate = rate; }
	// Bullets fired since the last Reset_ShotCount (the SHOOT_LOOP notifies fire them).
	int32 Get_ShotCount() const { return m_ShotCount; }
	void Reset_ShotCount() { m_ShotCount = 0; }
	// The middle of the hanging body, in world space (death blast, rubble aside).
	Vector3 Get_Center() const;
	void Off_Colliders() const;
	// Death burst, as Em0010Body: the pose freezes and the bone-driven parts fly apart and dissolve (Shader_VtxAnimMesh
	// pass Shatter). Returns the blast centre.
	Vector3 Begin_Shatter();
	Float Get_ShatterTime() const { return m_ShatterTime; }
	static constexpr Float ShatterDuration = 1.5f;	// fly, land and dissolve (Shader_VtxAnimMesh g_ShatterDuration)

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

private:
	// One machine-gun bullet from the head at the target's chest.
	void Fire_Bullet();
	// Which of the six stacked puppets this one is (m_Shown); the rest of em3004.model is skipped when drawing.
	void Pick_Puppet();
	HRESULT Bind_ShaderResources();
	HRESULT Ready_Components();
	HRESULT Ready_AnimationNotify();

private:
	Shared<Shader>	m_Shader{ nullptr };
	Shared<Model>	m_Model{ nullptr };
	Shared<AABBCollider> m_HitBox{ nullptr };
	Shared<SoundEmitter> m_SoundEmitter{ nullptr };
	int32 m_MuzzleBoneIndex{ -1 };
	int32 m_ShotCount{};
	Float m_AnimationRate{ 1.f };
	vector<Bool> m_Shown;					// per mesh: this puppet and the rubble it falls with
	Float m_ShatterTime{ -1.f };			// < 0 while intact
	Vector3 m_ShatterCenter{};
	vector<vector<Vector4>> m_PartCenters;	// per mesh, Model::Get_MeshPartCenters (Shader g_PartCenters[256])

	// em3004 has no gun bone and its seq names no muzzle; the shots leave from just in front of the head (bone5).
	static constexpr Float MuzzleReach = 0.3f;
	static constexpr Float ShotRange = 30.f;
	static constexpr int32 NumPuppets = 6;		// And_00~05; Gare_00~02 is the rubble (em3100's)

public:
	static Shared<Em3004Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
