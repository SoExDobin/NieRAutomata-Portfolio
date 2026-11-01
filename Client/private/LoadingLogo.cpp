#include "pch.h"
#include "LoadingLogo.h"
#include "Game.h"
#include "Shader.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"
#include <cmath>
void LoadingLogo::Update(Float timeDelta) {
    m_EffectTime += std::max(timeDelta, 0.f);
    UIImage::Update(timeDelta);
}
HRESULT LoadingLogo::Set_UVRect(const Float4& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) || !std::isfinite(value.w) ||
        value.x < 0.f || value.y < 0.f || value.z > 1.f || value.w > 1.f || value.z <= value.x || value.w <= value.y)
        return E_INVALIDARG;
    m_UVRect = value;
    return S_OK;
}
HRESULT LoadingLogo::Render() {
    const auto texture = Get_Component<Texture>();
    const auto shader = Get_Component<Shader>();
    const auto buffer = Get_Component<VIBuffer_Rect>();
    if (!texture || texture->Get_Textures().empty()) return S_OK;
    if (!shader || !buffer) return E_FAIL;
    Update_UITransform(1920.f, 1080.f);
    const Matrix view = Matrix::Identity;
    const Matrix projection = XMMatrixOrthographicLH(1920.f, 1080.f, 0.f, 1.f);
    if (FAILED(m_Transform->Bind_ShaderResource(shader, "g_WorldMatrix")) ||
        FAILED(shader->Bind_Matrix("g_ViewMatrix", &view)) || FAILED(shader->Bind_Matrix("g_ProjMatrix", &projection)) ||
        FAILED(texture->Bind_ShaderResourceView(shader, "g_Texture", 0)) ||
        FAILED(shader->Bind_RawValue("g_RGBA", texture->Get_RGBA_Absolute(), sizeof(Color))) ||
        FAILED(shader->Bind_RawValue("g_LoadingLogoUVRect", &m_UVRect, sizeof(m_UVRect))) ||
        FAILED(shader->Bind_RawValue("g_LoadingEffectTime", &m_EffectTime, sizeof(m_EffectTime))) ||
        FAILED(shader->Begin(2)) || FAILED(buffer->Bind_Resources())) return E_FAIL;
    return buffer->Render();
}
Shared<LoadingLogo> LoadingLogo::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) {
    auto result = make_shared<LoadingLogo>(device, context);
    return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}
Shared<GameObject> LoadingLogo::Clone(void* arg) {
    auto result = make_shared<LoadingLogo>(*this);
    return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
