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

// Em0000's skinned body: the model, the hit box, the attack volumes the clips switch on, and the crack-and-shatter death.
// A variant body (Emb001Body) wears another mesh of the family and notifies its own attack clips.
class CLIENT_DLL Em0000Body : public PartObject
{
public:
	typedef struct tagEm0000BodyDesc : public PARTOBJECT_DESC
	{
	} EM0000BODY_DESC;

public:
	explicit Em0000Body();
	explicit Em0000Body(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Em0000Body(const Em0000Body& rhs);
	~Em0000Body() override = default;

public:
	const TRANSFORM_FRAME& Get_ModelTransform() const;
	Shared<Model> Get_ModelComponent() { return m_Model; }
	void Set_Animation(uint32 animIndex, Float blendDuration, Bool isLoop);
	uint32 Get_CurrentAnimationIndex() const;
	Bool Is_AnimationFinished() const;
	Float Get_AnimationProgress() const;

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
	// Death: glowing fissures spread over the body until it bursts (Shader_VtxAnimMesh pass Crack, WORLDUI).
	void Begin_Crack() { if (m_CrackTime < 0.f) m_CrackTime = 0.f; }
	// Death burst: the pose freezes and the bone-driven parts scatter (pass Shatter). Returns where they fly out from.
	Vector3 Begin_Shatter();
	Float Get_ShatterTime() const { return m_ShatterTime; }
	static constexpr Float ShatterDuration = 1.5f;	// fly, land and dissolve (Shader_VtxAnimMesh g_ShatterDuration)

protected:
	// em0000; Level 4 registers the amusement-park em0003 as its replacement (ModelSettings "replaces").
	virtual const tChar* Get_ModelTag() const { return L"em0000"; }
	// The walk, dash and step-back clips' sounds (Em0000: em0000.bnk steps).
	virtual void Ready_MoveNotify();
	// The attack clips' sounds, telegraph and strike windows (Em0000: punch, swing, tackle).
	virtual void Ready_AttackNotify();
	// An attack clip's first frame: the warning sounds, the eyes flare and the streak crosses them (EmTelegraph).
	void Telegraph();
	// PAPER_THROW's two handfuls: a ConfettiEffect out of the raised hands.
	void Throw_Confetti();

private:
	// The eyes glow while the machine is awake and whole (not a dormant statue, not dying).
	Bool Is_EyeLit() const;
	HRESULT Bind_ShaderResources();
	HRESULT Render_Crack();
	HRESULT Ready_Components();
	HRESULT Ready_AnimationNotify();

protected:
	Shared<Model>			m_Model{ nullptr };
	Shared<SoundEmitter>	m_SoundEmitter{ nullptr };
	Weak<MonsterAOE>		m_LeftArm{};
	Weak<MonsterAOE>		m_RightArm{};
	Weak<MonsterAOE>		m_Torso{};

private:
	int32 m_RootBoneIndex{};
	Float m_ShatterTime{ -1.f };	// < 0 while intact
	Float m_CrackTime{ -1.f };		// < 0 while unharmed
	Float m_TellAge{ -1.f };		// since the last Telegraph, < 0 when none is running (EmTelegraph::Eye_Color)
	const Float n_CrackSpreadTime{ 1.f };
	// bone16 sits 0.216 m in front of the head's centre (bone3), the eyes' surface 0.47 m (em0000.blend bind pose).
	static constexpr Float EyeReach = 0.254f;
	vector<vector<Vector4>> m_PartCenters;	// per mesh, Model::Get_MeshPartCenters (Shader g_PartCenters[256])
	Vector3 m_ShatterCenter{};
	Shared<Shader>			m_Shader{ nullptr };
	Shared<AABBCollider>	m_HitBox{ nullptr };

public:
	static Shared<Em0000Body> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
