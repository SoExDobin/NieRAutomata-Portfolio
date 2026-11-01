#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VICube final : public VIBuffer
{
public:
	explicit VICube();
	explicit VICube(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit VICube(const VICube& rhs);
	virtual ~VICube() override = default;
	
public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::VI_CUBE_BUFFER; }
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg = nullptr) override;

public:
	static Shared<VICube> CreatePrototype();
	static Shared<VICube> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	Shared<Component> Clone(void* pArg) override;
	
};

NS_END