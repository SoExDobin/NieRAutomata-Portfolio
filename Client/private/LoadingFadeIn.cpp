#include "pch.h"
#include "LoadingFadeIn.h"
#include "Texture.h"
#include "Game.h"
#include "SpdLogger.h"
#include <filesystem>
#include <cmath>
LoadingFadeIn::LoadingFadeIn(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) : UIImage(device, context) {}
LoadingFadeIn::LoadingFadeIn(const LoadingFadeIn& rhs) : UIImage(rhs), m_Duration(rhs.m_Duration) {}
HRESULT LoadingFadeIn::Initialize(void* arg) {
    UI_IMAGE_DESC desc{};
    desc.sizeX = 1920.f; desc.sizeY = 1080.f;
    desc.textureTag = L"UI_HUD_Black";
    desc.color = Color(1.f, 1.f, 1.f, 0.f);
    if (arg) {
        const auto& old = *static_cast<FADE_IN_DESC*>(arg);
        if (old.fadeSpeed > 0.f) m_Duration = 1.f / old.fadeSpeed;
    }
    return UIImage::Initialize(&desc);
}
HRESULT LoadingFadeIn::Set_Duration(Float seconds) {
    if (!std::isfinite(seconds) || seconds < 0.f || seconds > 30.f) return E_INVALIDARG;
    m_Duration = seconds;
    return S_OK;
}
void LoadingFadeIn::Play(Bool cover) {
    const auto texture = Get_Component<Texture>();
    Color color = texture->Get_RGBAByValue();
    m_StartAlpha = cover ? color.w : 1.f;
    m_TargetAlpha = cover ? 1.f : 0.f;
    m_Elapsed = 0.f;
    m_Playing = m_Duration > 0.f && m_StartAlpha != m_TargetAlpha;
    color.w = m_Playing ? m_StartAlpha : m_TargetAlpha;
    texture->Set_RGBA(color);
    Set_Active(true);
}
void LoadingFadeIn::Update(Float timeDelta) {
    UIImage::Update(timeDelta);
    if (!m_Playing) return;
    m_Elapsed += std::max(timeDelta, 0.f);
    const Float t = m_Duration <= 0.f ? 1.f : std::min(m_Elapsed / m_Duration, 1.f);
    Color color = Get_Component<Texture>()->Get_RGBAByValue();
    color.w = m_StartAlpha + (m_TargetAlpha - m_StartAlpha) * t;
    Get_Component<Texture>()->Set_RGBA(color);
    m_Playing = t < 1.f;
}
void LoadingFadeIn::Submit_RenderGroup() { GAME_INSTANCE->Add_RenderGroup(RENDERGROUP::UI_OVERLAY, shared_from_this()); }
void LoadingFadeIn::Reset_Fade(Float speed) {
    if (speed > 0.f) Set_Duration(1.f / speed);
    auto color = Get_Component<Texture>()->Get_RGBAByValue(); color.w = 0.f;
    Get_Component<Texture>()->Set_RGBA(color);
    Play(true);
}
Shared<LoadingFadeIn> LoadingFadeIn::Load_UI(uint32 levelIndex) {
    for (const auto& [guid, path] : GAME_INSTANCE->Get_PrefabDocuments())
        if (std::filesystem::path(path).stem() == L"ScreenFade") {
            Shared<GameObject> root;
            if (SUCCEEDED(GAME_INSTANCE->DeSerializePrefabDocument(guid, root, levelIndex)))
                if (auto fade = Cast<LoadingFadeIn>(root)) return fade;
            if (root) root->Destroy_Subtree();
            break;
        }
    LOG_ERROR(L"[ScreenFade] ScreenFade prefab is missing or invalid");
    return nullptr;
}
Shared<LoadingFadeIn> LoadingFadeIn::Create(const ComPtr<ID3D11Device>& device, const ComPtr<ID3D11DeviceContext>& context) {
    auto result = make_shared<LoadingFadeIn>(device, context);
    return SUCCEEDED(result->Initialize_Prototype()) ? result : nullptr;
}
Shared<GameObject> LoadingFadeIn::Clone(void* arg) {
    auto result = make_shared<LoadingFadeIn>(*this);
    return SUCCEEDED(result->Initialize(arg)) ? result : nullptr;
}
