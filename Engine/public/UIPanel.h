#pragma once
#include "UIObject.h"

NS_BEGIN(Engine)

// A non-rendering, authored UI container. Children retain pixel dimensions.
class ENGINE_DLL UIPanel : public UIObject
{
public:
    UIPanel() = default;
    UIPanel(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    UIPanel(const UIPanel& rhs);
    HRESULT Initialize(void* arg) override;
    void Submit_RenderGroup() override;
    static Shared<UIPanel> CreatePrototype();
    Shared<GameObject> Clone(void* arg = nullptr) override;
};

NS_END
