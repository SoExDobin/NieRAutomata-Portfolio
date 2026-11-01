#pragma once
#include "GameObject.h"

NS_BEGIN(Engine)
class Model;
class Shader;
class VIBuffer_Particle_Point;
NS_END

NS_BEGIN(Client)

// Dodge streaks and animated body echoes, drawn at the dodge origin with the original material textures.
class CLIENT_DLL Pl0000EvadeGhost final : public GameObject
{
public:
	typedef struct tagEvadeGhostDesc : public GAMEOBJECT_DESC
	{
		vector<BONE_SNAPSHOT>	snapShots;
		Matrix					worldMatrix{};
		Float					lifeTime{ 0.5f };
		Vector4					ghostColor{ 1.f, 1.f, 1.f, 1.f };
		Bool						evadeEffect{ false };
		Bool						perfectEvade{ false };
		Bool						effectOnly{ false };
		Bool						followPose{ false };
		Vector3					travelDirection{ Vector3::Zero };
		Shared<Model>			model{ nullptr };
		Shared<Shader>			shader{ nullptr };
	} EVADE_GHOST_DESC;

	// Pass the shader side provides in Shader_VtxAnimMesh; reads g_GhostColor and g_GhostAlpha.
	static constexpr const Char* GhostPass = "PL_Ghost";

public:
	explicit Pl0000EvadeGhost();
	explicit Pl0000EvadeGhost(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit Pl0000EvadeGhost(const Pl0000EvadeGhost& rhs);
	~Pl0000EvadeGhost() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;

public:
	void Update(Float timeDelta) override;
	HRESULT Render() override;
	void Submit_RenderGroup() override;
	void Strengthen_PerfectEvade();

private:
	Shared<Shader>			m_RefShader{ nullptr };
	Shared<Model>			m_RefModel{ nullptr };
	Shared<Shader>			m_EffectShader{ nullptr };
	Shared<VIBuffer_Particle_Point> m_EffectBuffer{ nullptr };

	vector<BONE_SNAPSHOT>	m_SnapShots;
	Matrix					m_SpawnWorldMatrix{};
	Matrix					m_EffectWorldMatrix{};

	Float					m_LifeTime{ 0.f };
	Float					m_Elapsed{ 0.f };
	Float					m_Alpha{ 1.f };
	Vector4					m_GhostColor{ 1.f, 1.f, 1.f, 1.f };
	Float					m_EffectStrength{ 1.f };
	Bool					m_FollowPose{ false };
	Bool					m_DrawBody{ true };
	Vector3				m_Origin{ Vector3::Zero };
	Vector3				m_TravelDirection{ Vector3::Zero };
	Float					m_TravelDistance{ 2.6f };

public:
	static Shared<Pl0000EvadeGhost> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<GameObject> Clone(void* arg) override;
};

NS_END
