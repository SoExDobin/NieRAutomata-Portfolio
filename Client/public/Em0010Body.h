#pragma once
#include <AABBCollider.h>

#include "PartObject.h"

NS_BEGIN(Engine)
class Model;
class AABBCollider;
class SoundEmitter;
NS_END

NS_BEGIN(Client)

class MonsterAOE;

class CLIENT_DLL Em0010Body final : public PartObject
{
public:
	typedef struct tagEm0010BodyDesc : public PARTOBJECT_DESC
	{
		
	} EM0010BODY_DESC;

public:
	explicit Em0010Body();
	explicit Em0010Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em0010Body(const Em0010Body& rhs);
	~Em0010Body() override = default;
	
public:
	const TRANSFORM_FRAME& Get_ModelTransform() const;
	Shared<Model> Get_ModelComponent() { return m_Model; }
	void Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop);
	uint32 Get_CurrentAnimationIndex() const;
	uint32 Get_NextAnimationIndex() const;
	Float Get_AnimationProgress() const;
	Bool Is_AnimationFinished() const;

public:
	void Fire_Bullet(const Vector3& aimPosition);
	// A park visitor's salute: a firework shell from the barrel into the level's Firework sky (Firework::Launch).
	void Fire_Firework();

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	HRESULT Begin() override;
	void On_Destroy() override;

public:
	void Priority_Update(Float timeDelta) override;
	void Update(Float timeDelta) override;
	void Late_Update(Float timeDelta) override;
	void Fixed_Update(Float fixedDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;

public:
	// Death: the hit box and every attack volume stop colliding (an attack cut short never reaches its off notify).
	void Off_Colliders() const;
	// Death burst: the pose freezes and the bone-driven parts scatter and vanish (Shader_VtxAnimMesh pass Shatter).
	// Death: glowing fissures spread over the body until it bursts (Shader_VtxAnimMesh pass Crack, WORLDUI).
	void Begin_Crack() { if (m_CrackTime < 0.f) m_CrackTime = 0.f; }
	// Returns the world point the pieces fly out from.
	Vector3 Begin_Shatter();
	Float Get_ShatterTime() const { return m_ShatterTime; }
	static constexpr Float ShatterDuration = 1.5f;	// fly, land and dissolve (Shader_VtxAnimMesh g_ShatterDuration)
	void OnCollisionEnter(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionStay(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;
	void OnCollisionExit(const Shared<Collider>& ownCollider, const Shared<Collider>& targetCollider) override;

private:
	HRESULT Bind_ShaderResources();
	HRESULT Render_Crack();
	HRESULT Ready_Components();
	HRESULT Ready_AnimationNotify();

private:
	int32 m_RootBoneIndex{};
	int32 m_GunBoneIndex{ -1 };
	Float m_ShatterTime{ -1.f };	// < 0 while intact
	Float m_CrackTime{ -1.f };		// < 0 while unharmed
	Float m_TellAge{ -1.f };		// since the last telegraph, < 0 when none is running (EmTelegraph::Eye_Color)
	const Float n_CrackSpreadTime{ 1.f };
	vector<vector<Vector4>> m_PartCenters;	// per mesh, Model::Get_MeshPartCenters (Shader g_PartCenters[256])
	Vector3 m_ShatterCenter{};
	Shared<Shader>			m_Shader{ nullptr };
	Shared<Model>			m_Model{ nullptr };
	Shared<AABBCollider>	m_HitBox{ nullptr };
	Shared<SoundEmitter>	m_SoundEmitter{ nullptr };

	Weak<MonsterAOE>		m_LeftArm{};
	Weak<MonsterAOE>		m_RightArm{};
	Weak<MonsterAOE>		m_Foot{};

public:
	static Shared<Em0010Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
