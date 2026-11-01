#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Terrain final : public VIBuffer
{
public:
	typedef struct tagVIBufferTerrain : public VIBuffer
	{
		uint32 m_NumVerticesX{};
		uint32 m_NumVerticesZ{};
	} VIBUFFER_TERRAIN;

public:
	explicit VIBuffer_Terrain();
	explicit VIBuffer_Terrain(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit VIBuffer_Terrain(const VIBuffer_Terrain& rhs);
	~VIBuffer_Terrain() override = default;

public:
	HRESULT Initialize_Prototype(const tChar* pHeightMapFilePath = L"");
	HRESULT Initialize(void* arg = nullptr) override;

public:
	void On_Destroy() override { VIBuffer::On_Destroy(); }
	void On_Disable() override { VIBuffer::On_Disable(); }
	void On_Enable() override { VIBuffer::On_Enable(); }
	void Set_Active(Bool isActive) override { VIBuffer::Set_Active(isActive); }
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::VI_TERRAIN_BUFFER; }

private:
	uint32 m_NumVerticesX = {};
	uint32 m_NumVerticesZ = {};

public:
	static Shared<VIBuffer_Terrain> CreatePrototype();
	static Shared<VIBuffer_Terrain> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, const tChar* heightMapFilePath = L"");
	Shared<Component> Clone(void* arg = nullptr) override;
};

NS_END