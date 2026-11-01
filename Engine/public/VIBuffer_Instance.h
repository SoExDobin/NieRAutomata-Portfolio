#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Instance abstract : public VIBuffer
{

public:
	typedef struct tagInstanceDesc
	{
		uint32 numInstances{};
		Vector3 center{};
		Vector3 range{};
		Vector2 scale{}; // min, max
	} VIBUFFER_INSTANCE_DESC;

public:
	explicit VIBuffer_Instance();
	explicit VIBuffer_Instance(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit VIBuffer_Instance(const VIBuffer_Instance& rhs);
	virtual ~VIBuffer_Instance() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;
	HRESULT Bind_Resources() override;
	HRESULT Render() override;
	void On_Destroy() override;
	uint32 Get_NumInstances() const { return m_NumInstances; }

protected:
	ComPtr<ID3D11Buffer> m_VBInstance{ nullptr };
	uint32 m_InstanceStride{};
	uint32 m_NumInstances{};
	uint32 m_IndexCountPerInstance{};

public:
	virtual Shared<Component> Clone(void* arg = nullptr) override PURE;
	
};

NS_END