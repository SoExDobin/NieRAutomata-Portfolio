#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VISphere final : public VIBuffer
{
public:
	explicit VISphere();
	explicit VISphere(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	explicit VISphere(const VISphere& rhs);
	~VISphere() override = default;

public:
	COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::VI_SPHERE_BUFFER; }
	HRESULT Initialize_Prototype(uint32 numSlices, uint32 numStacks);
	HRESULT Initialize(void* arg = nullptr) override;

public:
	static Shared<VISphere> CreatePrototype();
	static Shared<VISphere> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context, uint32 numSlices = 20, uint32 numStacks = 20);
	Shared<Component> Clone(void* pArg) override;

};

NS_END
