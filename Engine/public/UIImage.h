#pragma once
#include "UIPanel.h"

NS_BEGIN(Engine)

// Generic authored image; resource and tint are owned by its Texture component.
class ENGINE_DLL UIImage : public UIPanel
{
public:
    // A code-built image (a HUD piece) names its texture and tint up front; the Editor's images start blank.
    typedef struct tagUIImageDesc : public UI_DESC
    {
        wstring textureTag;
        Color color{ 1.f, 1.f, 1.f, 1.f };
    } UI_IMAGE_DESC;

public:
    UIImage() = default;
    UIImage(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    UIImage(const UIImage& rhs);
    HRESULT Initialize(void* arg) override;
    HRESULT Render() override;
    void Submit_RenderGroup() override;
    static Shared<UIImage> CreatePrototype();
    Shared<GameObject> Clone(void* arg = nullptr) override;
};

NS_END
