#include "pch.h"
#include "LoadingFadeOut.h"
HRESULT LoadingFadeOut::Initialize(void* arg) {
    FADE_IN_DESC desc{};
    if (arg) desc.fadeSpeed = static_cast<FADE_OUT_DESC*>(arg)->fadeSpeed;
    if (FAILED(LoadingFadeIn::Initialize(arg ? &desc : nullptr))) return E_FAIL;
    Play(false);
    return S_OK;
}
Shared<LoadingFadeOut> LoadingFadeOut::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) {
    auto result = make_shared<LoadingFadeOut>(device, context);
    return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}
Shared<GameObject> LoadingFadeOut::Clone(void* arg) {
    auto result = make_shared<LoadingFadeOut>(*this);
    return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
