#pragma once
#include "VIBuffer.h"

NS_BEGIN(Engine)

class ENGINE_DLL VIBuffer_Rect final : public VIBuffer {
public:
    VIBuffer_Rect();
	VIBuffer_Rect(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
	VIBuffer_Rect(const VIBuffer_Rect& rhs);
	~VIBuffer_Rect() override = default;

public:
    COMPONENT_TYPE Get_ComponentType() const override { return COMPONENT_TYPE::VI_RECT_BUFFER; };
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void *arg) override;
    void On_Destroy() override { VIBuffer::On_Destroy(); }
    void On_Disable() override { VIBuffer::On_Disable(); }
    void On_Enable() override { VIBuffer::On_Enable(); }
    void Set_Active(Bool isActive) override { VIBuffer::Set_Active(isActive); }

public:
    static Shared<VIBuffer_Rect> CreatePrototype();
    static Shared<VIBuffer_Rect> Create(const ComPtr<ID3D11Device> &device, const ComPtr<ID3D11DeviceContext> &context);
    Shared<Component> Clone(void *arg) override;
};

NS_END