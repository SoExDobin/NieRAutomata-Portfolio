#pragma once
#include "Pl0000Parts.h"

NS_BEGIN(Engine)
class Model;
class AABBCollider;
class SoftBody;
NS_END

NS_BEGIN(Client)

class WP0220Body;
class WP0070Body;
class Pl0000EvadeGhost;
class WorldMap;

class CLIENT_DLL Pl0000Body final : public Pl0000Parts
{
public:
	typedef struct tagPl0000BodyDesc : public PL0000PART_DESC
	{} Pl0000BODY_DESC ;

public:
	explicit Pl0000Body();
	explicit Pl0000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000Body(const Pl0000Body& rhs);
	~Pl0000Body() override = default;

public:
	TRANSFORM_FRAME Get_ModelTransform() const { return m_Model->Get_RootTransformVelocity(m_RootBoneIndex); }
	// Leaves an afterimage of the current pose (Pl0000EvadeGhost).
	Shared<Pl0000EvadeGhost> Spawn_Ghost(Float lifeTime, const Vector4& color, Bool evadeEffect = false, Bool perfectEvade = false);
	Shared<Pl0000EvadeGhost> Spawn_EvadeStreaks();
	vector<Shared<Pl0000EvadeGhost>> Spawn_EvadeCross(Float lifeTime, const Vector4& color);

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
	HRESULT Bind_ShaderResources();
	HRESULT Ready_Components();
	HRESULT Ready_AnimationNotify();
	void Play_PendingFootsteps();
	// Notifies fire before Movement; consume at Late_Update after the parent's controller move.
	vector<wstring> m_PendingFootsteps;
	// This level's tiles with water, gathered on the first grounded footstep instead of scanning the level each step.
	vector<Weak<WorldMap>> m_WaterTiles;
	Bool m_WaterTilesGathered{ false };

private:
	int32					m_RootBoneIndex{};
	Matrix					m_SheathMatrix{};
	Shared<AABBCollider>	m_HitBox { nullptr };
	Shared<SoftBody>			m_SoftBody{ nullptr };

public:
	static Shared<Pl0000Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
