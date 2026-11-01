#pragma once
#include "UIImage.h"
NS_BEGIN(Client)
// The reference positions the emblem before the wordmark, so the editor selects each rectangle from one atlas.
class CLIENT_DLL LoadingLogo : public UIImage {
public:
    LoadingLogo() = default;
    LoadingLogo(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) : UIImage(device, context) {}
    LoadingLogo(const LoadingLogo& rhs) : UIImage(rhs), m_UVRect(rhs.m_UVRect) {}
    Float4 Get_UVRect() const { return m_UVRect; }
    HRESULT Set_UVRect(const Float4& value);
    void Update(Float timeDelta) override;
    HRESULT Render() override;
    static Shared<LoadingLogo> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    Shared<GameObject> Clone(void* arg) override;
private:
    Float4 m_UVRect{0.f, 0.f, 1.f, 1.f};
    Float m_EffectTime{};
};
NS_END
