#pragma once
#include "VIBuffer_Instance.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Particle_Point final : public VIBuffer_Instance
{
public:
	typedef struct tagParticlePointDesc : public VIBUFFER_INSTANCE_DESC
	{
		Vector3 pivot{};		// spread point
		Vector2 speed{};		// min max
		Vector2 lifeTime{};		// min max
		Bool isLoop{};
	} VIBUFFER_INSTANCE_POINT_DESC;
public:
	explicit VIBuffer_Particle_Point();
	explicit VIBuffer_Particle_Point(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit VIBuffer_Particle_Point(const VIBuffer_Particle_Point& rhs);
	~VIBuffer_Particle_Point() override = default;
	
public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	HRESULT Bind_Resources() override;
	HRESULT Render() override;
	// Draws only the first numInstances points (at most what the buffer holds): a pass that places each point itself
	// from its SV_InstanceID draws as many as it needs out of one buffer.
	HRESULT Render(uint32 numInstances);

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::VI_PARTICLE_POINT_BUFFER; }
	Bool Get_IsLoop() const { return m_IsLoop; }
	void On_Destroy() override;

public:
	void Update_Drop(Float timeDelta);
	void Update_Spread(Float timeDelta);

private:
	Bool							m_IsLoop{};
	Vector3							m_Pivot{};
	vector<VTXPARTICLE_INSTANCE>	m_InitialVertices;
	vector<VTXPARTICLE_INSTANCE>	m_CurrentVertices;
	vector<Vector3>			m_SpreadDirections;
	vector<Float>					m_Speeds;
	

public:
	static Shared<VIBuffer_Particle_Point> CreatePrototype();
	Shared<Component> Clone(void* arg) override;
};

NS_END
