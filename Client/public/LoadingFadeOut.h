#pragma once
#include "LoadingFadeIn.h"
NS_BEGIN(Client)
// Compatibility type for old editor scenes; both directions share LoadingFadeIn's implementation.
class CLIENT_DLL LoadingFadeOut final : public LoadingFadeIn {
public:
    struct FADE_OUT_DESC : UI_DESC { Float fadeSpeed{0.25f}; };
    LoadingFadeOut() = default;
    LoadingFadeOut(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) : LoadingFadeIn(device, context) {}
    LoadingFadeOut(const LoadingFadeOut& rhs) : LoadingFadeIn(rhs) {}
    HRESULT Initialize(void* arg) override;
    static Shared<LoadingFadeOut> Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context);
    Shared<GameObject> Clone(void* arg) override;
};
NS_END
